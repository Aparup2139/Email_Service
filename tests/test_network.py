"""Exercise real C processes and TCP peers using only the standard library."""

import argparse
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import tempfile
import time
import unittest


BIN_DIR = Path("build/normal").resolve()
TEST_PORT = None
MAX_PAYLOAD = 4096


def free_port():
    """Choose an available port; startup still checks for bind races."""
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def frame(data):
    """Encode a request in the same documented format as the C programs."""
    return struct.pack("!I", len(data)) + data


def read_exact(sock, size):
    """Read size bytes or fail clearly when a peer closes early."""
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise AssertionError("Peer closed before the expected bytes arrived")
        data.extend(chunk)
    return bytes(data)


def stop_process(proc):
    """Reap only this child; force termination if its bounded wait expires."""
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=7)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=3)


class Server:
    """Own one server, its output file, and its cleanup lifecycle."""

    def __init__(self):
        self.port = TEST_PORT if TEST_PORT is not None else free_port()
        self.proc = None
        self.temp = None
        self.out = None

    def log(self):
        return self.path.read_text(errors="replace")

    def wait_log(self, text, timeout=8):
        """Wait for actual process output rather than assuming startup speed."""
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if text in self.log():
                return
            if self.proc.poll() is not None:
                break
            time.sleep(0.01)
        raise AssertionError(f"Missing server output {text!r}:\n{self.log()}")

    def __enter__(self):
        self.temp = tempfile.TemporaryDirectory(prefix="email-test-")
        self.path = Path(self.temp.name) / "server.log"
        self.out = self.path.open("w")
        try:
            self.proc = subprocess.Popen(
                [str(BIN_DIR / "server"), "--port", str(self.port)],
                stdout=self.out, stderr=subprocess.STDOUT,
            )
            self.wait_log("Listening on")
            return self
        except BaseException:
            self.cleanup()
            raise

    def connect(self):
        sock = socket.create_connection(("127.0.0.1", self.port), timeout=8)
        sock.settimeout(8)
        return sock

    def client(self, message="Hello from the test"):
        return subprocess.run(
            [str(BIN_DIR / "client"), "--port", str(self.port),
             "--message", message],
            capture_output=True, text=True, timeout=12,
        )

    def cleanup(self):
        if self.proc is not None:
            stop_process(self.proc)
        if self.out is not None:
            self.out.close()
        if self.temp is not None:
            self.temp.cleanup()

    def __exit__(self, kind, value, trace):
        try:
            stop_process(self.proc)
            log = self.log()
            if kind is None:
                if self.proc.returncode != 0:
                    raise AssertionError(f"Server exit {self.proc.returncode}:\n{log}")
                if "AddressSanitizer" in log or "runtime error:" in log:
                    raise AssertionError(f"Sanitizer diagnostic:\n{log}")
        finally:
            self.cleanup()


class NetworkTests(unittest.TestCase):
    def assert_recovery(self, server):
        """A valid exchange proves more than merely checking process liveness."""
        result = server.client()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("response matches", result.stdout)
        self.assertIsNone(server.proc.poll())

    def test_sequential_clients(self):
        with Server() as server:
            for message in ("first", "second message", "third"):
                result = server.client(message)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("response matches", result.stdout)

    def test_binary_payload_one_buffer(self):
        with Server() as server, server.connect() as sock:
            data = b"\x00|\n%snul\xff\x00"
            sock.sendall(frame(data))
            self.assertEqual(read_exact(sock, 4 + len(data)), frame(data))
            self.assertEqual(sock.recv(1), b"")

    def test_fragmented_frame(self):
        with Server() as server, server.connect() as sock:
            data = b"fragmented header and body"
            for byte in frame(data):
                sock.sendall(bytes([byte]))
                time.sleep(0.005)
            self.assertEqual(read_exact(sock, 4 + len(data)), frame(data))

    def test_minimum_and_maximum_payload(self):
        with Server() as server:
            for data in (b"a", bytes(range(256)) * 16):
                with server.connect() as sock:
                    sock.sendall(frame(data))
                    self.assertEqual(read_exact(sock, 4 + len(data)), frame(data))
            result = server.client("x" * MAX_PAYLOAD)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_unicode_message_uses_byte_length(self):
        with Server() as server:
            result = server.client("Hello \u09ac\u09be\u0982\u09b2\u09be \u2709")
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_invalid_frame_lengths(self):
        with Server() as server:
            for size in (0, MAX_PAYLOAD + 1, 0xFFFFFFFF):
                with server.connect() as sock:
                    sock.sendall(struct.pack("!I", size))
                    self.assertEqual(sock.recv(1), b"")
                self.assert_recovery(server)
            self.assertIn("invalid frame length", server.log())

    def test_clean_eof_and_truncated_frames(self):
        cases = (
            (b"", "peer closed before a frame"),
            (b"\x00\x00", "truncated frame"),
            (struct.pack("!I", 10), "truncated frame"),
            (struct.pack("!I", 10) + b"abc", "truncated frame"),
        )
        for data, expected in cases:
            with self.subTest(data=data), Server() as server:
                with server.connect() as sock:
                    if data:
                        sock.sendall(data)
                    sock.shutdown(socket.SHUT_WR)
                    self.assertEqual(sock.recv(1), b"")
                self.assert_recovery(server)
                self.assertIn(expected, server.log())

    def test_reset_before_reading_response(self):
        with Server() as server:
            with server.connect() as sock:
                sock.sendall(frame(b"x" * MAX_PAYLOAD))
                # Linux linger=0 resets the connection on close.
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                struct.pack("ii", 1, 0))
            self.assert_recovery(server)

    def test_stalled_client_timeout_and_recovery(self):
        with Server() as server, server.connect() as sock:
            sock.sendall(b"\x00")
            start = time.monotonic()
            self.assertEqual(sock.recv(1), b"")
            elapsed = time.monotonic() - start
            self.assertGreaterEqual(elapsed, 4)
            self.assertLess(elapsed, 8)
            self.assert_recovery(server)
            self.assertIn("I/O timeout", server.log())

    def test_port_already_in_use(self):
        with Server() as server:
            result = subprocess.run(
                [str(BIN_DIR / "server"), "--port", str(server.port)],
                capture_output=True, text=True, timeout=8,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("bind", result.stderr)
            self.assert_recovery(server)

    def test_invalid_arguments(self):
        # Every rejected invocation must fail before network I/O is needed.
        cases = [
            ["--port", value] for value in
            ("", "0", "-1", "65536", "12x", "+12", " 12", "9" * 50)
        ]
        cases += [["--unknown"], ["--port"], ["extra"],
                  ["--port", "5050", "--port", "5051"]]
        for name, host_opt in (("server", "--bind"), ("client", "--host")):
            extra = ["--message", "ok"] if name == "client" else []
            invalid = cases + [[host_opt, "not-an-ip"], [host_opt, "::1"]]
            if name == "client":
                invalid += [["--message", ""], ["--message", "x" * 4097]]
            for args in invalid:
                # Message-specific cases already include their own option.
                tail = [] if "--message" in args else extra
                with self.subTest(name=name, args=args[:2]):
                    result = subprocess.run(
                        [str(BIN_DIR / name)] + args + tail,
                        capture_output=True, text=True, timeout=8,
                    )
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn("Invalid", result.stderr)
        result = subprocess.run([str(BIN_DIR / "client")],
                                capture_output=True, text=True, timeout=8)
        self.assertEqual(result.returncode, 1)

    def test_help(self):
        for name in ("server", "client"):
            result = subprocess.run([str(BIN_DIR / name), "--help"],
                                    capture_output=True, text=True, timeout=8)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Usage:", result.stdout)

    def test_unavailable_server(self):
        # Reserve a port without listening so another process cannot claim it.
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
            result = subprocess.run(
                [str(BIN_DIR / "client"), "--port", str(port), "--message", "hi"],
                capture_output=True, text=True, timeout=8,
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("connect", result.stderr)

    def run_controlled_peer(self, response):
        """Send a deliberate bad response, or stall until the client times out."""
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.settimeout(8)
            port = listener.getsockname()[1]
            proc = subprocess.Popen(
                [str(BIN_DIR / "client"), "--port", str(port), "--message", "hello"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            try:
                sock, _ = listener.accept()
                with sock:
                    sock.settimeout(8)
                    size = struct.unpack("!I", read_exact(sock, 4))[0]
                    self.assertEqual(read_exact(sock, size), b"hello")
                    if response is None:
                        self.assertEqual(sock.recv(1), b"")
                    else:
                        sock.sendall(response)
                        sock.shutdown(socket.SHUT_WR)
                    out, err = proc.communicate(timeout=8)
                self.assertEqual(proc.returncode, 1, out + err)
                self.assertIn("Exchange failed", err)
                return err
            finally:
                stop_process(proc)
                proc.communicate(timeout=3)

    def test_mismatched_response(self):
        self.assertIn("does not match", self.run_controlled_peer(frame(b"wrong")))

    def test_malformed_responses(self):
        for response in (struct.pack("!I", 0), struct.pack("!I", 4097),
                         b"\x00\x00", struct.pack("!I", 5) + b"ab", b""):
            with self.subTest(response=response):
                self.run_controlled_peer(response)

    def test_client_receive_timeout(self):
        self.assertIn("I/O timeout", self.run_controlled_peer(None))

    def test_idle_shutdown(self):
        for signum in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signum=signum), Server() as server:
                start = time.monotonic()
                server.proc.send_signal(signum)
                self.assertEqual(server.proc.wait(timeout=2), 0)
                self.assertLess(time.monotonic() - start, 2)

    def test_shutdown_with_stalled_client(self):
        for signum in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signum=signum), Server() as server:
                with server.connect() as sock:
                    sock.sendall(b"\x00")
                    server.wait_log("Client connected.")
                    start = time.monotonic()
                    server.proc.send_signal(signum)
                    self.assertEqual(server.proc.wait(timeout=7), 0)
                    self.assertLess(time.monotonic() - start, 7)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, default=BIN_DIR)
    parser.add_argument("--port", type=int, default=None,
                        help="fixed server test port; it must be available")
    args, remaining = parser.parse_known_args()
    if args.port is not None and not 1 <= args.port <= 65535:
        parser.error("--port must be in 1..65535")
    BIN_DIR = args.bin_dir.resolve()
    TEST_PORT = args.port
    for name in ("server", "client"):
        if not os.access(BIN_DIR / name, os.X_OK):
            parser.error(f"Missing executable: {BIN_DIR / name}; build it first")
    unittest.main(argv=[__file__] + remaining, verbosity=2)

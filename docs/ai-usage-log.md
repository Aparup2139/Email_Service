# AI usage log

This log records actual significant AI assistance. Entries separate checks performed by the AI agent from personal student verification. Tool names are known; a specific model version is not recorded. No claims of unaided authorship or student understanding are made.

## AI-001 Assignment analysis and implementation planning

- **Date:** 2026-11-01, Asia/Calcutta
- **Task:** Understand the assignment, choose a development sequence, and prepare the first milestone prompt.
- **AI Tool:** ChatGPT/Codex
- **Prompt/Question:** Student requested an assignment summary, technologies/libraries to learn, a step-by-step plan, and AI log setup; later requested `Plan/Step1.md` and readable naming/formatting/comments. See the [labelled interaction summary](ai-interactions/001-planning.md).
- **What Was Used:** Requirements analysis, recommended Linux/C networking foundation, staged plan, and the retained implementation prompt. The student chose the project directory and supplied the readability preference.
- **AI-performed checks:** Read the supplied assignment and inspected the project directory; verified prompt creation and update. No implementation tests occurred in this planning phase.
- **Student Verification:** Pending student review. Approval to implement is not a personal code-understanding or test result.

## AI-002 Networking foundation implementation and WSL verification

- **Date:** 2026-11-01, Asia/Calcutta
- **Task:** Implement Step 1 and verify execution through WSL.
- **AI Tool:** ChatGPT/Codex
- **Prompt/Question:** “Looks good implement this plan properly effectively and efficiently. WSL is installed in my system use that to verify the execution”. See the [request record](ai-interactions/002-implementation.md) and [full implementation prompt snapshot](ai-interactions/002-implementation-prompt.md).
- **What Was Used:** AI-generated `src/server.c`, `src/client.c`, `src/net_io.c`, `include/net_io.h`, `Makefile`, `.gitignore`, `tests/test_network.py`, README, protocol/architecture notes, and evidence/log documents. Git was initialised without commits or a remote.
- **AI-performed checks:** Environment inspection, warning-enabled builds, real TCP integration tests, and ASan/UBSan verification in Ubuntu-24.04. See [actual results and raw evidence](test-results.md) for commands, outcomes, and limitations. The first environment probe selected Docker's default distribution; the agent corrected this by explicitly selecting Ubuntu-24.04. GDB was unavailable and not used.
- **Student Verification:** Pending student review. The student has not yet reported running the demonstration, explaining the code, or making a verified modification.
- **Design choices evaluated:** Kept this milestone sequential; used bounded binary framing and a shared I/O module; kept later account/storage/concurrency features pending. No claims of complete reliability, fabricated screenshots, or fabricated test results are included.

## Adding future entries

Record significant assistance when it happens. Include all six assignment fields: Date, Task, AI Tool, Prompt/Question, What Was Used, and Student Verification. Save long prompts or a clearly labelled summary/reference under `ai-interactions/`. Add evidence, modifications, rejected suggestions, and commit links when they actually exist. Initially write `Pending student review`; replace it only with the student's real explanation or experiment and actual observed outcome.

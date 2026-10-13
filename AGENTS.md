# AGENTS.md — Offline UPI Mesh

## Mission
Improve the reliability, correctness, security, performance, and clarity of this offline/deferred-payment prototype. Treat payment and ledger invariants as the highest priority. Do not describe this prototype as production-ready or as a real UPI/banking system unless that has been independently demonstrated.

## Repository map
- `src/main.cpp`: Crow HTTP routes and ingestion flow.
- `src/crypto/`: hybrid RSA-OAEP + AES-GCM packet encryption/decryption.
- `src/service/`: in-memory idempotency cache.
- `src/db/`: SQLite schema and settlement logic.
- `include/upi_mesh/`: public headers.
- `tests/`: C++ tests.
- `upi-frontend/`: static HTML/CSS/JS debugging dashboard.
- `CMakeLists.txt`: build and dependency configuration.
- `scaling_roadmap.md`: proposed evolution; distinguish roadmap ideas from implemented behavior.

## Non-negotiable invariants
1. A packet can settle at most once, including after process restarts and concurrent deliveries.
2. Sender debit, receiver credit, and ledger insertion succeed or fail together in one database transaction.
3. Amounts must be strictly positive, within explicit bounds, and represented without integer overflow.
4. The sender and receiver must be valid existing accounts; verify that each update affects exactly one row.
5. Never build SQL with untrusted strings. Use prepared statements and bound parameters.
6. Treat all HTTP input, decrypted packet fields, API errors, and frontend values as untrusted.
7. Authenticate packet contents and define a versioned schema; distinguish packet identifiers from ciphertext hashes and bind identifiers to authenticated data.
8. A failed/transient attempt must not permanently poison retry state. Define pending, succeeded, and retryable-failure semantics.
9. Bound request/body/ciphertext sizes before parsing, allocating large buffers, or performing expensive cryptographic work.
10. Keep deduplication memory bounded. The database is the source of truth; an in-memory cache may only be an optimization.

## Working process
- Inspect the relevant implementation and tests before proposing or changing behavior.
- Check open and closed GitHub issues and PRs before filing a new issue; link or update an existing one instead of duplicating it.
- For issues, include precise affected paths, a concrete failure/reproduction scenario, impact, expected behavior, and testable acceptance criteria. Label severity conservatively.
- Prefer small, reviewable changes with tests. Do not silently broaden scope or rewrite unrelated modules.
- Do not claim a bug was reproduced unless you actually ran a reproducer. Mark code-inspection findings as such.
- Do not claim performance improvements without before/after measurements and a reproducible benchmark setup.

## Build and test
- Use the documented CMake configuration and run the C++ tests after changes.
- Add tests for both success and failure paths. For settlement changes, verify balances and ledger rows in a temporary SQLite database.
- Exercise concurrent duplicate deliveries, restart/replay behavior, retry after recoverable failure, invalid payloads, and account/amount validation.
- Integrate tests with CTest/CI where practical so they run automatically.

## Cryptography and memory safety
- Check every OpenSSL allocation and return value; free resources on all error paths (prefer RAII wrappers).
- Validate Base64 decoding strictly and validate exact packet framing/key lengths before slicing buffers.
- Check conversions from `size_t` to OpenSSL APIs' integer lengths for overflow/truncation.
- Never log plaintext payment payloads, private keys, or sensitive account data.
- Keep cryptographic algorithm parameters explicit and covered by tampering and malformed-input tests.

## HTTP and frontend behavior
- Use explicit request-size limits, stable response schemas/status codes, and safe error messages.
- Review CORS and deployment assumptions; wildcard origins should not be treated as a production security boundary.
- Frontend logs should render API-derived text as text, not HTML. Avoid `innerHTML` with interpolated data.
- Handle network errors, non-JSON responses, timeouts, loading states, and retry UX without losing pending packets.
- Keep the demo dashboard clearly marked as a simulator unless actual offline-device transport and settlement integration exists.

## Dependencies and builds
- Avoid floating dependency refs such as Git branches in reproducible builds; prefer pinned releases/commits after compatibility testing.
- Keep compiler warnings enabled and address new warnings. Use sanitizers and fuzz malformed packet parsing when feasible.
- Never commit credentials, private keys, generated databases, or local build artifacts.

## Issue priority guidance
- High: ledger corruption, double settlement, injection, invalid transaction acceptance, unrecoverable packet loss/replay, or severe resource-exhaustion paths.
- Medium: robustness, retry UX, unbounded resource use, protocol validation, dashboard reliability.
- Low: documentation, maintainability, non-blocking polish, and missing coverage after critical invariants are protected.

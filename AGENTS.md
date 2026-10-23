# Repository instructions

- Before changing build, packaging, dependencies, release artifacts, installer behavior, architecture paths, or files under `scripts/`, `cmake/`, `packaging/`, read `docs/build-and-packaging.md` completely.
- Treat `docs/build-and-packaging.md` as the canonical description of the supported build and packaging flow. Update it in the same commit whenever that flow changes.
- Keep code, documentation, commit messages, package metadata, and repository automation in English.
- Use Conventional Commits.
- Supported release packages are Windows MSI and Debian DEB for x64/amd64 and ARM64. Do not add setup wrappers, AppImage, WinGet, or APT repository automation unless explicitly requested.
- Keep generated files out of Git. `build/` is for compiler state, `stage/` is the exact install payload, and `dist/` is only for final distributable packages.
- Never commit private signing keys, certificates, tokens, profiles, or locally downloaded tools.

## Development workflow

- For product changes, follow this cycle: analyze the existing behavior; write or update the specification and acceptance criteria; implement the smallest correct change; run focused tests; benchmark and profile performance-sensitive changes; debug failures to their root cause; run the relevant CI-equivalent checks; review the diff for correctness, security, regressions, and unnecessary complexity; update documentation; then prepare a release only when explicitly requested or when the task is a release task.
- Scale every phase to the change. Documentation-only or trivial changes may record a phase as not applicable; do not invent benchmarks, profiling, or release work when they add no evidence.
- Keep planning and implementation separate. The implementation must follow the accepted specification, and any discovered scope change must be reflected in the specification before continuing.
- Prefer focused local checks during implementation. Before release, run the complete supported test/build/package matrix: Windows x64, Windows ARM64, Linux amd64, and Linux arm64. A change may skip a target only when it demonstrably cannot affect that target, and the final handoff must state the reason. Treat failures as work to diagnose and fix, not as checks to bypass.
- In the final handoff, report what changed, which tests and CI-equivalent checks ran, benchmark or profiling results when applicable, review findings, documentation updates, and any remaining release risks.

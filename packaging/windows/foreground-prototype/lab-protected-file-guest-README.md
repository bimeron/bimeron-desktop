# Protected-file guest harness

This offline lab is separate from MSI and production. Acceptance criteria:

- The executable accepts only `--verify-only` or the exact three arguments `--guest-pending-file-once --lab-only DisposableWindowsVM`. Verification performs no file operations. Other arguments fail before file operations.
- The guest mode requires its exact executable path under native Program Files: `BimeronProtectedFileLab\lab-protected-file-guest.exe`. It never creates directories. A separately authorized operator provisions that directory and its `LABProtectedFiles` child with trusted BA/SY/exact TI ownership and protected ACLs, and no standard-user write access. Program Files and its root are unchanged.
- An existing `LABProtectedFiles\pending.bin` stops the run without modification. A successful run leaves a new Pending record with fresh nonzero random IDs and counter 1. Existing-file create, stale installation, stale generation, and a sharing conflict must refuse; the record remains byte-identical after every refusal. No cleanup, repair, deletion, reparse or hardlink mutation is performed.
- JSON output records bounded hexadecimal IDs and counter only. The offline check statically inspects the frozen x64 executable with a reviewed DLL/API allowlist and produces hashes and a fixed-effects manifest under new ignored build state. It never executes the harness or builds either architecture. It does not run guest mode from Program Files or provision anything.
- Native success requires separate explicit permission and a disposable Windows VM. This is not evidence of MSI integration, callbacks, or client authority. Rebuild and rehash after any primitive change before requesting that permission.

The operator must first review the final manifest and separately obtain this
permission: "I authorize provisioning the two fixed lab directories and copying
the hashed harness in a disposable Windows VM, then running it once to create
and retain `LABProtectedFiles\pending.bin`, perform one acknowledgment and the
specified stale-ID, existing-file, and sharing-conflict refusals. No automatic
cleanup is authorized." Provision trusted protected ACLs as specified above;
do not change the Program Files directory or volume root. Verify the copied
executable hash against the final manifest and confirm pending.bin is absent.
The authorized guest command is:

```powershell
& "$env:ProgramFiles\BimeronProtectedFileLab\lab-protected-file-guest.exe" --guest-pending-file-once --lab-only DisposableWindowsVM
```

Retain the resulting file and bounded JSON output as evidence. Any failure may
leave a new or partially written pending file; stop and preserve it. Never retry
or repair it automatically. The final x64 permission manifest has `provisional: false`, `scope: x64-only`,
and `completeArchitectureMatrix: false`. ARM64 is unbuilt for this bundle; that
does not block x64 permission preparation or imply full release validation.
Historical host argument/path refusal results are not rerun by static preparation.

## Static preparation correction acceptance (recorded before implementation)

Preparation must never execute the harness, rebuild it, provision a guest, or
alter the old provisional bundle. Inspect the frozen x64 hash with an explicit
reviewed DLL/API allowlist; reject unknown DLLs/APIs and delay imports. Exercise
negative parser fixtures without executing the harness. Bind unchanged behavior
sources and updated preparation sources separately in a new final x64-only
permission manifest. ARM64 remains unbuilt for this bundle and does not block
finalizing x64 permission; this is not the complete release architecture matrix.
Independent review and explicit user approval of the concrete final bundle remain
required before any guest provisioning or execution.

Static preparation command (existing local dumpbin, no harness execution):

```powershell
./packaging/windows/foreground-prototype/lab-protected-file-guest-check.ps1 -Dumpbin '<absolute dumpbin.exe path>' -FrozenBundlePath '<existing frozen guest-bundle directory>'
```

The checker preserves the old provisional bundle and refuses to overwrite
`build/guest-bundle-final-x64-reviewed`. It checks the four unchanged build/behavior hashes
against all six entries in the old manifest, records the two updated preparation
hashes, and adds the explicit allowlist hash. No rebuild is needed for these
preparation-only changes. The checked-in allowlist explains each DLL group;
static CRT imports include dynamic loading APIs. Direct-import inspection cannot
prove absence of dynamic loading; source inspection found no arbitrary dynamic
path in the guest/primitive sources. Negative parser fixtures reject unknown
DLLs, unknown APIs, and delay imports without running the harness.

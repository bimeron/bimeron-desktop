# Protected-file guest harness

This offline lab is separate from MSI and production. Acceptance criteria:

- The executable accepts only `--verify-only` or the exact three arguments `--guest-pending-file-once --lab-only DisposableWindowsVM`. Verification performs no file operations. Other arguments fail before file operations.
- The guest mode requires its exact executable path under native Program Files: `BimeronProtectedFileLab\lab-protected-file-guest.exe`. It never creates directories. A separately authorized operator provisions that directory and its `LABProtectedFiles` child with trusted BA/SY/exact TI ownership and protected ACLs, and no standard-user write access. Program Files and its root are unchanged.
- An existing `LABProtectedFiles\pending.bin` stops the run without modification. A successful run leaves a new Pending record with fresh nonzero random IDs and counter 1. Existing-file create, stale installation, stale generation, and a sharing conflict must refuse; the record remains byte-identical after every refusal. No cleanup, repair, deletion, reparse or hardlink mutation is performed.
- JSON output records bounded hexadecimal IDs and counter only. The offline check builds x64 and cross-compiles ARM64 explicitly, checks imports and argument/path refusals, and produces hashes and a fixed-effects manifest under ignored build state. It does not run guest mode from Program Files or provision anything.
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
or repair it automatically. The manifest is provisional until the parallel
primitive changes are integrated and both architectures are rebuilt and checked.
`-Architectures x64` can produce an explicitly incomplete local manifest when
ARM64 tools are unavailable; omit it for the full check.

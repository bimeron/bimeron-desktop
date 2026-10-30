# Offline foreground transaction feasibility experiment

This standalone project is deliberately absent from root CMake and CPack. Never
install its DLL. `ForegroundPrototype` returns `ERROR_INSTALL_FAILURE` on every
call, including nested uninstall. The generated WiX source embeds it as a Binary,
not an installed Engine helper. The fixture is not a proposed production sequence.

From the repository root, using installed tools only:

```powershell
cmake -S packaging/windows/foreground-prototype -B build/foreground-prototype-amd64 -G "Visual Studio 18 2026" -A x64
cmake --build build/foreground-prototype-amd64 --config Release
ctest --test-dir build/foreground-prototype-amd64 -C Release --output-on-failure
cmake -S packaging/windows/foreground-prototype -B build/foreground-prototype-arm64-tools -G "Visual Studio 18 2026" -A ARM64 "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools"
cmake --build build/foreground-prototype-arm64-tools --config Release
ctest --test-dir build/foreground-prototype-arm64-tools -C Release -R foreground-sequence --output-on-failure
```

The x64 worker test is intentionally unelevated. It creates unique Local objects
and a temporary directory. Child processes only probe the fixture mutex or exit
deliberately while owning it; no owner process, network, Global object or installed
policy is touched. An unexpected test failure can leave its named temporary
directory for inspection. ARM64 is compile-only on this host. Generated sources,
DLLs, objects and any tools belong in ignored build state.

`admit` ports the immutable Core internal guard: inspect actual kernel-object
owner/DACL, zero-timeout wait, release/refuse abandonment and treat only marker
ERROR_FILE_NOT_FOUND as absent. Caller must retain ownership on that same thread.
The standalone worker tests exercise mechanics with user-owned fixture handles;
they deliberately cannot pass production admission. Descriptor fixtures test the
trusted SYSTEM/Administrators contract without changing host ACLs or elevating.

The worker uses a serialized in-process mailbox. One OS thread acquires/releases
the fixture mutex and refuses a second begin, incomplete settlement and existing
journals. A process-lifetime DLL pin protects instructions, not the process itself.
Commit/rollback signals simulate externally completed work; no actual MSI rollback
is performed. The pending journal preserves opaque installation/policy/rollback
test bytes and is flushed before begin succeeds. It is not a protected production
journal or policy schema implementation. Teardown releases the fixture mutex but
retains the pending record; abrupt child death also retains it. No automatic
repair/retry follows.

The generated WiX check proves an unconditional checked refusal occurs before
InstallInitialize and the fixture's afterInstallInitialize major-upgrade removal.
It checks Binary binding and architecture and rejects five mutated sequences.
It also verifies the embedded DLL's actual PE machine matches that architecture.
WiX 3 candle compilation additionally checks syntax. Neither proves deferred
execution, rollback/commit order, callback identity, or host lifetime.

## Handoff and integration gate

The concrete failure is reproduced by `nested.begin()` and the separate-process
probe: an independent callback worker cannot acquire the mutex held by the outer
worker. It needs authenticated routing to that exact owner, never shared handle
storage, release/reacquire, an unauthenticated public endpoint or a general helper.
No secure cross-process protocol or MSI lifetime contract was established here.
Keep the production gate closed.

Before integrating, obtain disposable actual-MSI evidence for clean install,
repair, removal, major upgrade with nested old uninstall, failure at each mutation,
completed rollback/commit, callback process termination, reboot and sharing
violations. Establish authenticated callback authorization and bounded failure
handling while the worker remains continuously alive and owns the guard. Journal
settlement must reflect completed restoration/publication, not callback return.
Never kill owners or schedule replacement outside the guard.

Then implement protected native Program Files path/ancestor, reparse, ownership,
effective DACL and pinned handle checks for actual Core/GUI/policy/journal paths.
Require approved architecture-specific final Core inputs; hash the actual final
installed GUI. Publish only schema 1 with a cryptographically random public
16-byte lowercase-hex ID, preserving it and exact previous policy/rollback metadata
through repair/upgrade. Flush a protected durable journal before any destructive
MSI action and refuse all incomplete records after crashes/reboots. Release pinned
handles and exclusion only after completed settlement. Review generated MSI tables
for all destructive actions, including RemoveExistingProducts, and ensure both
architectures embed their matching DLL. This prototype authorizes none of those
production mutations.

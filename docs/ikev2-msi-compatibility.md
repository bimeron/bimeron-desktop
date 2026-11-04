# Offline MSI compatibility inventory

## Accepted bounded specification (before implementation)

2026-11-04, planned with codex_petr / gpt-6-astra / medium. This implements only
the independent artifact-inventory slice A of the completed 267-line
`plan-ikev2-safe-maintenance/docs/ikev2-safe-maintenance-plan.md` in the sibling
Desktop worktree (thread `mcp:9ad00c16-2df5-47bc-a020-0586aa1ce4a4`). It does not
replace that plan. Consumer B is accepted; native enablement remains OFF.

Own branch: `feat/ikev2-maintenance-compatibility`, clean starting HEAD
`38e020eb41bfb619f2a794e9628e3d0fc6002a40`, equal to local
`plan/ikev2-installation-contract`. T3 confirms this checkout and codex_petr /
gpt-6-astra / medium. Parent drafts were read explicitly from the separate
`plan-ikev2-installation-contract/docs` directory, including accepted Step 5,
maintenance boundaries and later lab outcomes; they were not copied or edited.
Core inspection uses `498596eabe4ec734cb0bda72059dd74a66c9dc49` plus dirty
lifecycle fixes, not approved packaged bytes. No Core changes belong here.

### Smallest implementation

Add one standard-library Python offline inspection utility and focused tests.
On Windows it may open an explicitly named MSI database READONLY and query tables,
summary metadata and embedded stream hashes. Never open an installer session,
execute MSI/custom actions, query installed products/registry, extract-and-run a
helper, or invoke install/repair/uninstall. Do not download tooling or dependencies.
Saved projections must also be checkable without Windows or an MSI DLL.

Report exact source SHA-256/size, ProductVersion, ProductCode, PackageCode,
UpgradeCode and platform, with table names and a bounded projection of Property,
Directory, Component, File, RemoveFile, Registry, RemoveRegistry, ServiceInstall,
ServiceControl, CustomAction, all authored sequence tables, Upgrade, LaunchCondition,
Media and UI action invocation metadata. Distinguish absent optional tables from
failed reads. Include embedded Binary hashes where present, without executing them.
Preserve raw conditions and sequence numbers; do not pretend to evaluate arbitrary
MSI conditions or determine installed effective state from a source package.

Check projection structure, required metadata, duplicate identifiers, references
needed for file paths, supported architecture and schema/bounds. Unknown or
unsupported input/errors must fail closed for the check, never become empty safe
tables. Report unknown tables/actions as unresolved review requirements. Successful
inventory is not maintenance safety approval: every result explicitly leaves
native eligibility false. A named prospective gate alone cannot prove coordination.
Use explicit errors rather than optimization-removable assertions in the utility.

Inventory available historical product packages read-only, deduplicating exact
hashes. Initial locations: the main checkout's dist and saved VM/test MSI paths,
and the release-desktop-0.2.0-no-ikev2 worktree's dist and saved public-release
verification directories. Ignore standalone lab MSIs as product baselines. Limit
any additional public release retrieval to official repository assets, bounded
size and concrete missing historical versions; local artifacts are preferred.
Generated exports belong only in this checkout's ignored build directory.
Record compact identities/findings here, distinguish locally observed bytes from
release provenance claims and documentation-only history. No installed cache,
transform/patch state, actual executor token or historical helper behavior is
established by source-package inspection alone.

### Acceptance

1. Actual local MSI metadata is read without installer execution, and package
   identity/hash and ordered mutation actions are recorded reproducibly.
2. Representative sanitized fixtures exercise the real parser/checker, including
   duplicate/malformed/missing fields, unknown schema/action/table, unsupported
   architecture, missing file/component/directory references and unknown conditions.
   Preserve conservative unknown verdicts rather than constant-only model tests.
3. Inspect current options.wxs and CMake package definitions in focused regression
   checks so the distinction between UI launch and execute-sequence admission is
   tied to real source. Do not introduce production gate wiring.
4. Source metadata establishes whether inspected old sequences contain mutations
   without a lifecycle gate. Explain direct/quiet/repair/cached/uninstall/nested
   rollback boundaries without claiming that every route was executed or that a
   future gate retroactively protects old packages.
5. Run focused tests, CLI error paths and git diff --check, then a sequential
   focused review. No heavy builds or native matrix. Benchmarks/profiling and the
   four-target product release matrix are inapplicable to this offline-only slice;
   release safety remains unproved. Keep all changes uncommitted.

### Required runtime follow-up (not implemented)

Core service_update_windows.go currently owns BimeronCoreUpdate, repairs the service
directory DACL and stages .update-new before stop. service_windows.go uninstall
accepts SCM Stopped then removes SCM/event-log registration before file cleanup.
BeginInstallationTransaction owns BimeronCoreConnect and checks ForegroundOwner,
but has no production servicing caller and supplies no durable cleanup proof.
Future Core work must define common admission before those first mutations, lock
order/lifetime through rollback, positive owned RAS/DNS/WFP closure and persistent
uncertainty refusal, plus retained recovery bytes/state and retirement authority.
Stopped, owner absence, empty enumeration and successful MSI exit are insufficient.

Future MSI work must cover execute-sequence entry, shutdown, old-product removal,
quiet/repair/uninstall and rollback using the real Core authority once available.
Old cached MSI cannot acquire new checks retroactively. Prove harmless isolation
or an enforceable clean migration for ordinary servicing, otherwise keep native
off. No mandatory host policy, new broker or universal admin-blocking guarantee
is selected here. Existing signed packet updater verification must be preserved.

## Observed local source artifacts (2026-11-04)

Read-only msi.dll database queries inspected six unique hashes; duplicate paths in public/remote verification directories were deduplicated. All report ProductVersion 0.2.0 and UpgradeCode `{E93DE42D-1F4F-4227-8056-1086581CCE51}`. These are locally observed source bytes, not independently authenticated release provenance. No 0.1.x package was present in the supplied locations; documentation history does not fill that gap. Raw projections and location list are in ignored `build/msi-inventory/`.

- SHA-256 `18015aa11a83b408fbe2c2b2bd74e39b1f38d469a124865c70e5c397b3188a46`, 475136 bytes, x64;1033; ProductCode `{B5EFC6BF-B628-4AC0-A5B3-9212FBEE4644}`, PackageCode `{B50C8BA9-CE30-457F-9037-16207724E400}`.
- SHA-256 `6f11d7b240ccd78843d676c9ee489d9f924d685254d8a7e8b431930a89e987fd`, 30720000 bytes, x64;1033; ProductCode `{9FE64B75-ABAA-4272-8AB9-59A892457511}`, PackageCode `{3E6626ED-C53F-459F-BDEA-1DB6BA1FF761}`.
- SHA-256 `80aec88310194ab75c75982e458407754ac2a1bb535ded8afbe86257ced5b8a7`, 20463616 bytes, Arm64;1033; ProductCode `{81C1084C-27D3-4F47-AEA6-290990B8C687}`, PackageCode `{6AD46DB1-F954-4BFE-912C-54790C4C1BE5}`.
- SHA-256 `a80d49355ab3f840ab11bf1aef650b9dcb36475fb4d14d996b6d49c7bfac4e51`, 475136 bytes, x64;1033; ProductCode `{005C2704-C48D-4799-BC71-3029E1125A0A}`, PackageCode `{D9DC6750-E354-4C85-8BFA-29106F48B856}`.
- SHA-256 `aee9dfad204ee68d477b14fd9da84a692ba67e7f8853cde9107f51afd4ef92fb`, 30720000 bytes, x64;1033; ProductCode `{385E739F-568F-4C2E-83AF-A9CE825E0DD7}`, PackageCode `{00701BFE-46B9-4BB6-8822-BD914E1032D1}`.
- SHA-256 `db2c925afe2ec39fcf142002427a014d61880ec364bcb60982005de7cbf36ccb`, 20463616 bytes, Arm64;1033; ProductCode `{2F9C6EFB-E429-4DCF-860D-C69D68720A52}`, PackageCode `{A00CE6B6-2DFC-4B15-B330-4D3E2944061D}`.

### Authored sequences and targets

The following execute sequence is shared by the six inspected projections (conditions retained as authored). No lifecycle admission custom action appears. Sequence order is metadata, not observed execution.

```text
25 FindRelatedProducts []
50 AppSearch []
100 LaunchConditions []
700 ValidateProductID []
800 CostInitialize []
900 FileCost []
1000 CostFinalize []
1001 SetARPINSTALLLOCATION []
1200 MigrateFeatureStates []
1400 InstallValidate []
1500 InstallInitialize []
1501 RemoveExistingProducts []
1600 ProcessComponents []
1800 UnpublishFeatures []
2600 RemoveRegistryValues []
3200 RemoveShortcuts []
3500 RemoveFiles []
4000 InstallFiles []
4500 CreateShortcuts []
5000 WriteRegistryValues []
6000 RegisterUser []
6100 RegisterProduct []
6300 PublishFeatures []
6400 PublishProduct []
6600 InstallFinalize []
```

Custom actions (identical across inspected packages):
```json
[
  {
    "Action": "LaunchBimeron",
    "Type": "226",
    "Source": "INSTALL_ROOT",
    "Target": "\"[INSTALL_ROOT]bimeron-desktop.exe\"",
    "ExtendedType": ""
  },
  {
    "Action": "WixUIPrintEula",
    "Type": "65",
    "Source": "WixUIWixca",
    "Target": "PrintEula",
    "ExtendedType": ""
  },
  {
    "Action": "SetARPINSTALLLOCATION",
    "Type": "51",
    "Source": "ARPINSTALLLOCATION",
    "Target": "[INSTALL_ROOT]",
    "ExtendedType": ""
  }
]
```

File targets include the bundled GUI, bimeron-core.exe and wintun.dll under the authored install directory; Registry/RemoveFile rows cover shortcut registration and folder removal. Full Directory/Component/File references and UI ControlEvent conditions are retained in projections. UI LaunchBimeron is a finish-dialog action, not execute-sequence admission.

Direct/quiet entry can reach authored execute actions without that UI event. Repair, uninstall and cached maintenance depend on effective installed metadata and conditions; neither those routes nor nested old-product removal and rollback were executed here. A future new-package gate cannot retroactively protect an old cached package. Transforms, patches, installed cache, executor token and helper behavior remain unknown. Native eligibility is always false.

### Validation and review

Focused unittest suite: four tests pass normally and under Python -O, including
12 malformed/missing/reference/architecture/schema mutations, unknown review
requirements, preserved unknown conditions, and current WiX/CMake source checks.
All six real saved projections pass structural checks. Missing-file CLI returns
failure with native eligibility false. `git diff --check` passes; generated output
is ignored. Sequential review checked READONLY-only native API usage, handle
closure, query/stream bounds, source hash stability and separation of structural
success from unresolved compatibility. No installer action, registry/product-state
query, helper, network, VM, heavy build, commit or release was performed.
Windows x64/ARM64 and Linux amd64/arm64 product build/package checks and performance
profiling are inapplicable: only the offline utility/tests/docs changed. Python
cross-host saved-projection logic is covered locally; no Linux runtime was used.
Remaining evidence gaps include 0.1.x source packages, release provenance, installed
transforms/cache state, lifecycle coordination and all native route acceptance.

## Accepted independent-review correction refinement (before implementation)

Pin the supported projected table layouts (ordered column numbers, names, types
and keys) independently of input, using the six observed identical schemas.
Reject other projected schemas, including unobserved service/removal schemas.
Validate required values, integer fields, references, exact summary fields and
Binary SHA-256 format. Replace skeletal fixtures with sanitized representative
rows using actual layouts. Read Summary WordCount through the read-only API;
record per-hash source locations here. Extracted stage artifacts are derived with
unknown original provenance. Cabinet names alone establish neither original nor
administrative-image status. Flags cannot prove installed cache/effective state
or release authenticity. Regression checks include self-described schema attacks,
summary/digest failures, six regenerated projections, optimized mode and CLI errors.
The earlier sequential review was implementation self-review, not independent review.

### Source path ledger after read-only regeneration

- `aee9dfad204ee68d477b14fd9da84a692ba67e7f8853cde9107f51afd4ef92fb`: `C:\Users\PC\Documents\dev\bimeron\bimeron-desktop\dist\bimeron-desktop-0.2.0-windows-amd64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `80aec88310194ab75c75982e458407754ac2a1bb535ded8afbe86257ced5b8a7`: `C:\Users\PC\Documents\dev\bimeron\bimeron-desktop\dist\bimeron-desktop-0.2.0-windows-arm64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `18015aa11a83b408fbe2c2b2bd74e39b1f38d469a124865c70e5c397b3188a46`: `C:\Users\PC\Documents\dev\bimeron\bimeron-desktop\stage\vm-msi-extracted-020\bimeron-desktop-0.2.0-windows-amd64.msi`; WordCount=4. Derived extracted-stage artifact; original provenance unknown.
- `a80d49355ab3f840ab11bf1aef650b9dcb36475fb4d14d996b6d49c7bfac4e51`: `C:\Users\PC\Documents\dev\bimeron\bimeron-desktop\stage\local-update-test-msi-extracted\bimeron-desktop-0.2.0-windows-amd64-local-update-test.msi`; WordCount=4. Derived extracted-stage artifact; original provenance unknown.
- `6f11d7b240ccd78843d676c9ee489d9f924d685254d8a7e8b431930a89e987fd`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\dist\bimeron-desktop-0.2.0-windows-amd64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `db2c925afe2ec39fcf142002427a014d61880ec364bcb60982005de7cbf36ccb`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\dist\bimeron-desktop-0.2.0-windows-arm64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `6f11d7b240ccd78843d676c9ee489d9f924d685254d8a7e8b431930a89e987fd`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\build\public-desktop-0.2.0-verify-902a7d8397fa49968c82ac465de201db\bimeron-desktop-0.2.0-windows-amd64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `db2c925afe2ec39fcf142002427a014d61880ec364bcb60982005de7cbf36ccb`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\build\public-desktop-0.2.0-verify-902a7d8397fa49968c82ac465de201db\bimeron-desktop-0.2.0-windows-arm64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `6f11d7b240ccd78843d676c9ee489d9f924d685254d8a7e8b431930a89e987fd`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\build\remote-desktop-0.2.0-verify-b622882ff768462da1263ebcacb1a6ad\bimeron-desktop-0.2.0-windows-amd64.msi`; WordCount=2. Locally observed source; release authenticity unverified.
- `db2c925afe2ec39fcf142002427a014d61880ec364bcb60982005de7cbf36ccb`: `C:\Users\PC\.t3\worktrees\bimeron-desktop\release-desktop-0.2.0-no-ikev2\build\remote-desktop-0.2.0-verify-b622882ff768462da1263ebcacb1a6ad\bimeron-desktop-0.2.0-windows-arm64.msi`; WordCount=2. Locally observed source; release authenticity unverified.

### Correction results and review response

Independent review P1/P2 findings accepted: schema is now pinned independently,
summary fields are exact, Binary digests validated, and source flags/locations
recorded above. Five focused tests pass normally and under optimization; all six
regenerated projections pass the CLI. Missing-file CLI exit 1 verified. The old
cycle regression initially failed because actual row ordering differs from the
skeletal fixture; it now explicitly creates a self-cycle. Implementation self-review
checked read-only APIs, schema refusal and component key-path references. This is
not a new independent review. Native eligibility remains false. Product matrix,
benchmarks and profiling remain inapplicable; no release or installed-state proof.

Final independent correction review (codex_petr / gpt-6-astra / low) found no
blocking findings in this bounded slice and confirmed all three prior findings
resolved. It reran five tests normally and with `-O`, all six projection checks,
and whitespace checks. The coordinator also reran both test modes. Coverage is
limited to the observed schemas; unobserved service/removal table layouts refuse.
The fixture contains 81 columns and 378 representative rows. No installer session
or action API is used. Generated Python caches remain untracked: automatic command
policy rejected their cleanup. They are not deliverables and must not be committed.
Source authenticity, installed state and runtime maintenance safety remain unproved.

# IKEv2 committed-source integration order

Planning snapshot: 2026-11-05. Proposal only; no merge, cherry-pick, implementation,
staging, commit, push or release is authorized by this document. Native remains
OFF. Error 775 is unresolved; error 668 normal cleanup failed. Neither is evidence
of a successful protected IKEv2 connection.

## Specification and acceptance

Inventory the supplied immutable commits, distinguish repository histories,
identify source dependencies and likely conflicts, and propose finite integration
checks. Acceptance is a traceable order with explicit approval/evidence gaps, no
runtime work and no dependency on dirty lab sources. Existing GUI, Core auth and
MSI specifications/tests remain authoritative for their bounded slices; do not
duplicate their APIs or expand their scope here.

## Verified inventory

- Desktop GUI: `feat/ikev2-gui-connection-feedback` at
  `dbcf2dc109d63366e3e3d526d4e4c29c04089089`. Its chain already contains
  `1fb7157e6cb51b17d87a79bd85ca2e1d3c16892d` (post-D3 backlog),
  `4edb869` (offline provisioning action adapter), `49263d5` (local profile
  creation) and `2632568` (native profile/authentication UI). Do not replay D3 or
  the backlog separately after integrating this chain. The final 775 label is
  fixture-only: current transport has no structured 775 producer.
- Desktop MSI: `feat/ikev2-maintenance-compatibility` at
  `e26e2f71d723ce20b6631e583452da44545500ab`. One offline inventory commit adds
  `scripts/msi_inventory.py`, a saved projection fixture, Python tests and
  documentation. It proves neither installed-state compatibility nor maintenance
  admission. Native eligibility remains false even for structurally valid input.
- These Desktop branches share base
  `38e020eb41bfb619f2a794e9628e3d0fc6002a40`. Their changed-file sets from that
  base are disjoint. This is an overlap check, not a merge simulation or proof
  that integration is correct.
- Core auth: separate **bimeron-core** repository,
  `feat/ikev2-auth-contract` at
  `8ef0110326c86c1621a0c274dfb06a30b123324b`, parent
  `498596eabe4ec734cb0bda72059dd74a66c9dc49`. It adds a private disabled
  in-process seam, lifecycle retirement hooks, false public capabilities and
  offline tests. There is no production consumer, secret wire schema, Windows
  credential/provider interaction or persistence implementation. Inspect its
  committed `docs/IKEV2_AUTH_CONTRACT.md`; never merge/cherry-pick this hash into
  Desktop. Dirty Core lifecycle fixes are excluded from this inventory.

## Proposed source integration order

1. The owner chooses and freezes destination refs and grants integration authority.
   Local Desktop `main` was `296130e3e5f255410dd70c0680b37b0ed351165b`;
   its merge base with both feature tips is `da6ecac`. Core local `master` was
   `df7f2fd32a69efc49018e58efcc8d0f4f4c4e697`; no destination approval or
   Core feature/destination conflict analysis is implied. Revalidate refs before
   future edits; do not fetch or inspect runtime state as part of this plan.
2. Integrate the offline MSI inventory slice into the chosen Desktop destination
   first, after review. This small independent slice establishes historical
   evidence tooling, not a runtime prerequisite for the GUI. Preserve its canonical
   build-document addition; it does not change production installer sequencing.
3. Integrate the Desktop GUI chain once, preserving its disabled production
   boundary. A full branch includes lifecycle planning/prototype ancestry before
   the feature commits; review the full destination-to-tip delta before selecting
   merge versus an explicitly approved dependent commit series. Do not blindly
   replay only the latest feedback commit onto main without its UI/Session base.
4. Integrate Core auth separately into the approved Core destination, with that
   repository's review and tests. It may proceed independently of steps 2/3; there
   is no cross-repository Git ordering dependency. Record the final two repository
   SHAs together without inventing a compatible runtime pair or changing bundle pins.
5. Stop at offline source acceptance. Any error-wire contract, controller binding,
   credential producer, maintenance admission/recovery or native enablement is a
   later specification and authorization, not a follow-on merge in this task.

## Conflict and semantic review

Current Desktop main changes `MainWindow`, `ProfileEditorDialog`, translations,
`ConnectionManagementTest.h` and `ProfileVlessTest.cpp`, also touched by the GUI
chain. Review profile import/editor unification, ShadowTLS draft preservation and
Core repair/service diagnostics together with native draft handling and packet
dispatch isolation. Retain test registration and existing packet/GeoData behavior;
do not resolve conflicts by replacing entire files with a feature version.

MSI inventory overlaps main in `docs/build-and-packaging.md`; preserve supported
MSI/DEB targets and separate historical lab/model evidence from shipping behavior.
Absence of feature-to-feature file overlap does not remove these destination risks.
Core lifecycle changes can overlap `foreground.go`, `server.go` and capabilities;
the approved destination must retain auth retirement on loss/stop/close while
preserving foreground owner lifetime. No dirty-source comparison is permitted.

## Checks for a future authorized integration

- Recheck exact refs, clean isolated checkout and ancestry; inspect full diff and
  any conflict resolutions. Reuse existing tests rather than add another model.
- Desktop GUI: explicit offscreen Qt environment, optional external Core inputs
  cleared; focused `developer-mode-test` and `profile-vless-test`, plus existing
  GeoData regression tests when shared paths change. Check translation JSON,
  native disablement, no production controller binding and fixture-only 775.
- MSI: existing `tests/test_msi_inventory.py` in normal and optimized Python,
  saved sanitized JSON only. Do not invoke native MSI reads, installed product
  queries or historical artifact discovery under the present restrictions.
- Core: on a separately authorized isolated committed checkout, existing control
  tests, focused auth race tests, vet and bounded capability/config regressions
  listed in its auth document. Verify selected tests do not invoke native APIs.
- Check whitespace and review security, regressions and unnecessary complexity.
  Prior committed test reports are historical evidence, not fresh integrated runs.
  Full four-target build/package validation belongs before a separately authorized
  release; ARM64 runtime evidence cannot be replaced by cross-compilation.

## Missing decisions and evidence

Destination/merge strategy approval, final conflict review and fresh integrated
tests remain missing. Runtime additionally needs an accepted structured error
contract, real authenticated production producers, supported Windows auth method
and attempt-only/storage semantics, maintenance exclusion before mutation, owned
RAS/DNS/WFP cleanup/recovery and retained recovery bytes. Historical MSI coverage,
provenance and installed cache/transform behavior remain incomplete. Match final
approved Core/GUI artifacts and revision requirements before any bundle change;
the current Session revision guard is not satisfied merely by merging Core auth.
775 diagnosis and 668 cleanup failure remain unresolved independently of these
offline branches. No lab retry or cleanup is authorized here.

## This task's validation and handoff

T3 verified this worktree/branch, `codex_petr/gpt-6.1-sol` at medium effort, and
initial clean Git HEAD equal to the GUI tip. Read repository instructions and the
canonical build document; inspected committed histories, file deltas, existing
specifications/tests and Core objects through its root Git database only. No
active Core installation-contract checkout or ignored build state was read.
Only this plan is added. Whitespace and reference checks are the relevant local
checks; product tests, CI builds, four-target packages, benchmarks/profiling and
release work are inapplicable to this documentation-only task. No runtime or
integration success is claimed. Proposed future commit title:
`docs(ikev2): record committed branch integration order`.

## Integration record (2026-11-05)

Local branch `integrate/ikev2-desktop`, intended as a fast-forward of
`origin/plan/stage-8-ikev2` (`943c92d`); nothing is pushed by this record.

- `chore/ikev2-plans-errors-commits` (`dbcf2dc`..`3962951`) is the base.
- `chore/ikev2-msi-history-commit` (`a3b8558`, containing `e26e2f7`) and
  `feat/ikev2-safe-update-msi` (`0093248`) were added with two `--no-ff` merge
  commits. Both merged without conflicts; the only shared file,
  `docs/build-and-packaging.md`, received disjoint hunks.
- Pending: `fix/ikev2-test-teardown-geodata-exclude` (tip `bf78f85` when
  recorded) is still in progress in another thread. Merge its final tip into
  this branch with a normal merge commit once accepted; a dry run of the
  recorded tip merged without conflicts.

Offline verification at merge commit `81d39f1` (GUI-only, no Core fetch):

- Windows x64 (VS 2026 Build Tools, Qt 6.8.3): build passed; CTest 5/6.
  `developer-mode-test` aborts at "Supported clipboard shows Paste" (native
  clipboard fixture from `dbcf2dc`); unchanged by the MSI merges.
- Linux amd64 (WSL Ubuntu 24.04, Qt 6.4.2): build passed; CTest 4/6.
  `developer-mode-test` and `profile-vless-test` crash with SIGSEGV during
  widget destruction, before their remaining checks run. Local bisection:
  `943c92d` and `da6ecac` pass, merge `38e020e` crashes (developer-mode-test
  3/3, profile-vless-test 1/3), so the defect comes from combining the main and
  IKEv2 sides, not from the MSI merges. CI has not run `38e020e` yet.
- `tests/test_msi_inventory.py` passed with `python -B` and `python -B -O`; it is
  not part of CTest or Desktop CI.

No package, ARM64 build, benchmark or release work was performed.

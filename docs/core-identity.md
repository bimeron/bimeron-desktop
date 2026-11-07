# Core build identity

Tracking: [Desktop #26](https://github.com/bimeron/bimeron-desktop/issues/26).

About shows which Core build the GUI is using and marks builds that are not official releases. Two identities are involved:

- the runtime Core: the installed service or, in Development, the local daemon, as reported by IPC `hello`;
- the validation CLI: the executable next to the GUI (or the Development Core) as reported by its `version` command. Profile validation and the profile editor's document bridge use it.

Both identities pass through the existing sanitizer in `MainWindow.cpp` before classification. Classification lives in `src/CoreIdentity.h`.

## Classification

| Result | Rule |
| --- | --- |
| Release | Version without `-dev`, a 40-character lowercase revision and `modified=false`. If the version equals the pinned bundled release, the revision must equal `BIMERON_CORE_REVISION`. |
| Development | Any other reported identity: a `-dev` version, a missing or abbreviated revision, a modified tree, a missing modification flag, a rejected version string, or the pinned version built from another commit. |
| Unknown | No identity was reported. |

Every official Core release from `v0.1.0-beta.1` reports its full revision and `modified=false`, through both `version` and IPC `hello`.

The GUI only knows the commit of the pinned release. A newer or older signed release is accepted as a release from its reported properties alone; the GUI does not verify those revisions offline. A deliberately forged identity can therefore pass as a release. The marker is a development aid, not a security boundary: signed updates and the trusted helper remain the integrity controls.

Go records the revision only when it builds inside a regular Git checkout. Builds from a linked `git worktree`, from a source archive or with `-buildvcs=false` carry no revision and are shown as `dev-core` without a commit.

## Display

The About Core label shows:

- a release: its version, for example `Core: 0.3.0-beta.2`;
- a development build: `Core: dev-core (6b5340a)` with the first seven characters of the revision, or `Core: dev-core` without one;
- no identity: `Core: unknown`.

When the validation CLI is a different build from the runtime Core (version, revision or modification flag differs), the label appends ` · validation CLI: <label>`, for example `Core: 0.3.0-beta.2 · validation CLI: dev-core (6b5340a)`. While the runtime Core is unknown, the suffix appears only for a development CLI, so a stopped service with the bundled CLI still reads `Core: unknown`.

The marker appears in every build, including packages, because a user can select a local Core in Development. The tooltip keeps the full version, revision, platform, architecture and modification state of both identities.

The signed update check still receives the raw runtime version; this display change does not alter update eligibility.

## Pinned release

`cmake/core-release.cmake` pins `BIMERON_CORE_VERSION` and its commit `BIMERON_CORE_REVISION`. CMake passes both to the GUI as `BIMERON_PINNED_CORE_VERSION` and `BIMERON_PINNED_CORE_REVISION`. Package configuration verifies the commit against the pinned binary when that binary runs on the build host; see [Building and packaging](build-and-packaging.md#updating-the-bundled-core-version).

## Acceptance criteria

- The pinned release with its pinned revision shows its version.
- The pinned version with another revision, a modified build, a missing revision or a `-dev` version shows `dev-core`.
- A newer or older release with a full revision and an unmodified tree shows its version.
- No identity shows `unknown`.
- The validation suffix appears only when the validation CLI differs from the runtime Core, as described above.
- `core-identity-test` covers these cases and the pinned definitions; a wrong `BIMERON_CORE_REVISION` stops package configuration on a host that can run the pinned binary.

## Verification

Windows amd64, 2026-11-08: the full CTest suite passed (6 tests, including `core-identity-test`). Package configuration with a deliberately wrong `BIMERON_CORE_REVISION` failed with the reported pinned revision `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`; the correct pin configured normally. In the running GUI, About showed `Core: dev-core (55ddd84) · CLI проверки: dev-core (6b5340a)` with a locally built installed service (`0.1.0-dev`, modified) and a validation CLI built from Core `master` `6b5340a1`.

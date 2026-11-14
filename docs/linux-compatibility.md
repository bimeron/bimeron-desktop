# Linux package compatibility

## Specification

Desktop 0.3.0 Linux release packages target Ubuntu 22.04's system Qt 6.2,
glibc 2.35 and GCC 11 runtime baseline on amd64 and arm64. Build release DEBs
against that baseline rather than lowering metadata on binaries built on a newer
system. Keep system Qt, DEB packaging, Core pins and the existing privileged
repair model; no bundled Qt, new package format or third-party runtime repository.

Newer Debian/Ubuntu systems with compatible system libraries are intended targets;
this does not claim Ubuntu 18.04/20.04 support, other distribution package formats
or validation of every derivative. Windows retains its existing Qt/toolchain.

## Acceptance

- Configure and compile with the stock Ubuntu 22.04 Qt 6.2.4 and GCC 11.
- Both DEBs declare dependencies compatible with the baseline and include actual
  ELF runtime dependencies; no newer Qt/glibc/libstdc++ symbol requirement is hidden.
- Install the amd64 DEB using ordinary Ubuntu 22.04 repositories, launch the real
  GUI under the existing desktop user and run the native test suite.
- Check Core, repair broker, plugins, translations and GeoData, including their
  pinned bytes and protected paths. Real polkit/repair acceptance remains required.
- Build/package arm64 against the same baseline; report runtime limits explicitly.
- Run Windows regression checks for any shared source changes; preserve MSI flow.

This is a compatibility/build change, not a performance-sensitive algorithm.
Benchmarks/profiling are not applicable unless a regression appears. Existing
release artifacts are superseded only after successful new package verification.

Qt 6.2 lacks QFormLayout::setRowVisible. The existing widget-row helper hides
both label and field on that version; newer Qt retains the current API and the
6.4.0-6.4.2 upstream-bug workaround. Existing editor geometry/toggle fixtures
must pass on both Qt generations. Linux packaging checks ELF symbol versions
before staging and derives runtime library dependencies with dpkg-shlibdeps.
Native amd64 CI uses Ubuntu 22.04; arm64 CI on Ubuntu 24.04 additionally checks
newer system Qt compatibility, without claiming baseline package acceptance.

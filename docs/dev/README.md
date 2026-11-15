# Internal documentation

Specifications, plans and verification records kept for maintainers. They are historical:
statements about the current Core, open work or test status describe the time each document was
written. For current behavior, start with the [documentation index](../README.md).

## Contracts and specifications

- [Core/Desktop integration contract](core-desktop-integration-contract.md)
- [Cross-platform GUI profile editing contract](gui-profile-editing-contract.md)
- [Core build identity](core-identity.md)
- [Dynamic key access denied](access-key-failure.md)
- [Desktop navigation acceptance criteria](navigation-spec.md)
- [Bimeron for macOS: port specification](macos-app-spec.md) (not implemented)

## Interface notes

- [Application display name](application-display-name.md)
- [Appearance theme](windows-dark-theme.md)
- [Language selector](language-selector.md)
- [Profile list labels](profile-list.md)
- HiDPI rendering: [checkbox](checkbox-hidpi.md), [combo box](combo-box-hidpi.md),
  [connection button](connection-button-hidpi.md), [profile list frame](profile-list-hidpi.md),
  [connection status font](connection-status-font.md)

## Plans

- [Core availability and recovery](core-recovery-plan.md)
- [Settings layout and startup selection](settings-layout-plan.md)
- GeoData: [routing](geodata-routing-plan.md), [default databases](geodata-defaults-plan.md),
  [download dialog](geodata-download-plan.md), [database manager redesign](geodata-manager-redesign-plan.md),
  [GeoSite list order](geosite-list-order-plan.md)

## Verification records

- [GeoData implementation verification](geodata-routing-verification.md)
- [Windows VM check for Desktop 0.2.0 and Core v0.2.0-beta.1](vm-core-0.2.0-beta.1-check.md)

## Native IKEv2 (experimental, not a user feature)

The native IKEv2 code is hidden outside developer mode. These records track its design and
offline experiments.

- [Experimental native IKEv2 Desktop integration](../ikev2-profiles.md) (kept in `docs/` because
  other repository files link to it)
- [Integration order](ikev2-integration-order.md)
- [GUI integration plan](ikev2-gui-integration-plan.md)
- [Protected installation contract](ikev2-installation-contract.md)
- [Minimal Windows acceptance proposal](ikev2-minimal-acceptance.md)
- [Safe update and MSI maintenance](ikev2-safe-update-msi.md)
- [Offline MSI compatibility inventory](ikev2-msi-compatibility.md)
- [Safe error transport proposal](ikev2-error-contract-plan.md)
- [Offline failure parser](ikev2-offline-error-parser.md)
- [Lab handoff, 2026-11-01](ikev2-lab-handoff.md)

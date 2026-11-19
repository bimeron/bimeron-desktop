# Bimeron Desktop documentation

These documents describe how Bimeron Desktop behaves and how it is built. Most of them are
specifications written while each feature was developed: Core versions, stage numbers and test
results inside them describe the state at that time. The Core bundled with the current release
is listed in its [release notes](release-notes-0.3.0.md).

Start with the [repository README](../README.md) for installation and an overview.

## Releases and platforms

- [Desktop 0.3.0 release notes](release-notes-0.3.0.md)
- [Linux package compatibility](linux-compatibility.md): the Ubuntu 22.04 baseline
- [Building and packaging](build-and-packaging.md): requirements, MSI and DEB contents, release
  checks
- Release acceptance records: [0.3.0](release-0.3.0.md), [0.2.0](release-0.2.0.md)

## Profiles

- [Unified profile creation](profile-creation.md): adding profiles from links and files
- [Protocol URI import](protocol-uri-import.md)
- [Parameter forms and ShadowTLS](parameter-forms-and-shadowtls.md)
- [Profile preservation and secret handling](profile-preservation.md)
- Protocols: [Shadowsocks](shadowsocks-profiles.md), [VLESS](vless-profiles.md),
  [Hysteria2](hysteria2-profiles.md), [remote SOCKS5](remote-socks5.md), [SSH](ssh-profiles.md)

## Connection and Core service

- [Connection management](connection-management.md)
- [Bounded reconnect recovery](reconnect-attempts.md)
- [Connection repair and service actions](service-actions.md)
- [Control server identity](control-server-identity.md): verifying the Core endpoint before any request
- [Core error presentation](core-error-presentation.md)
- [Signed Core service updates](core-updates.md)
- [Tray interaction](tray-interaction.md)

## Routing

- [GeoData routing](geodata-routing.md): country and site rules, databases and updates
- [GeoSite picker trial](geosite-picker-trial.md): design notes for the site group list

## Development

- [Developer mode](developer-mode.md): a session-only mode for testing a local Core build
- [Internal specifications, plans and verification records](dev/README.md)

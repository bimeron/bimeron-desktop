Bimeron Desktop 0.3.1 pre-release for Windows and Debian/Ubuntu, amd64 and ARM64.

## Highlights

- Server identity check: Bimeron verifies that the local Core control endpoint belongs to the Bimeron Core service before it sends any request, including profiles.
- Endless reconnection after a lost connection, with a visible attempt counter, a pause countdown and a "Retry now" button.
- Kill switch hold: with the kill switch enabled for a profile, internet access stays blocked between reconnection attempts until the VPN connects again or you select Disconnect VPN.
- Trust for the new Core release signing keys, so Desktop 0.3.1 accepts Core updates signed from `v0.4.0-beta.3` onward.

## Changes

- Server identity check. On Windows the service pipe must be owned by `LocalSystem` and served by the running `BimeronCore` service process. On Linux the control socket must sit in a root-owned runtime directory and its peer must be root. If the check fails, nothing is sent, the connection is refused with `control_server_untrusted`, and Bimeron does not fall back to another endpoint.
- Reconnect policy, decided and reported by Core:
  - a new connection makes the first attempt plus up to 3 retries, shown as `Connecting… (n/3)`;
  - after an established connection is lost, Core retries in endless cycles of 10 attempts, shown as `Reconnecting… (n/10)`, with a 60-second pause between cycles (`Server unavailable · retrying in 0:59`) and the cause of the last error;
  - "Retry now" skips the pause, and Disconnect VPN stops reconnecting;
  - tray notifications report a lost connection once per cycle and say "VPN reconnected" when it recovers.
- Kill switch hold. While Core holds protection, the window shows "Internet access is paused until the VPN connects." and the main button becomes Disconnect VPN, which releases protection. After a failed first connection with the kill switch, internet access also stays paused until you select Disconnect VPN. The kill switch remains optional per profile and is still off by default.
- Recovery from leftover protection. If a service crash or a power loss leaves the kill switch filters behind, Bimeron shows "Internet is blocked by leftover VPN protection." and offers Fix connection, which runs the emergency network recovery. Connect is blocked until then.
- Core update warning. When a Core update would interrupt a session protected by the kill switch, the "Core update ready" dialog warns that leak protection will be removed during the update and that internet traffic will bypass the VPN.
- Release key rotation. Desktop 0.3.1 trusts three Core update signing keys: the current key `7370a2d8cf890e5e`, which signed every Core release up to `v0.4.0-beta.2`, and two hardware keys, `e6fb20d55656904a` (primary) and `095d295f5fe9a4e2` (backup), which sign Core from `v0.4.0-beta.3` onward. Earlier Desktop releases do not trust the new keys and will not receive those Core updates; upgrade to Desktop 0.3.1.

## Fixes

- A local process that created the Core control endpoint before the service could receive Desktop requests. Desktop now verifies the server first (see Changes).
- A repeated start of a profile that is already running no longer shows an error.

## Bundled Core

Bimeron Core `v0.4.0-beta.2`, commit `5251bd09`. It adds the endless reconnect policy with kill switch hold, control server identity hardening and Go security updates to `v0.4.0-beta.1`.

## Packages

Choose the MSI for Windows or the DEB for Debian/Ubuntu matching your architecture. SHA-256:

| File | SHA-256 |
|---|---|
| `bimeron-desktop-0.3.1-windows-amd64.msi` | `9d76b824e0df1a4f9110976ab110c4b08bf93d6347419c6d3493e28200bd73d7` |
| `bimeron-desktop-0.3.1-windows-arm64.msi` | `8664c06ed1d6dc84cc4d906ec75d7ef89842ae1c8ba46ad398163cf77ef96149` |
| `bimeron-desktop-0.3.1-linux-amd64.deb` | `2133c4915f876c518dd7be2a7c37abfd847bc757d11cc3b8986dc9bee7027cbc` |
| `bimeron-desktop-0.3.1-linux-arm64.deb` | `f7d5bc4081d58495dc68d9aad6ef90f0dcbfbbccf051fc2a4db00867a405c0eb` |

The packages are not code-signed. Windows may show an unknown-publisher warning; verify the SHA-256 above before installing.

## Upgrade notes

- Upgrade from Desktop 0.3.0 or 0.2.0 by installing the matching 0.3.1 package over it. Profiles and settings are kept.
- If the installed Core service is older than the bundled Core, the installer updates it to `v0.4.0-beta.2`. A service that already runs the same build is left untouched and is not restarted. An active VPN connection is interrupted during a service update; reconnect afterwards.
- Desktop 0.3.0 users should upgrade: Desktop 0.3.0 has no server identity check.

## Uninstalling

Removing the Desktop package keeps the Bimeron Core service installed and running. To remove the service too, run `bimeron-core service stop` and then `bimeron-core service uninstall` from an administrator command prompt (Windows, in `C:\Program Files\Bimeron`) or with `sudo` (Linux) before removing the Desktop package. User profiles and settings are kept.

## Known limitations

- This is a pre-release.
- Kill switch protection does not survive a normal reboot or shutdown, or a stop of the Bimeron Core service: these release it, and after the next boot traffic bypasses the VPN until you connect again. "Always-on VPN at boot" is planned for the next release.
- Leftover blocking (see Recovery from leftover protection) appears only after a service crash or a power loss.
- A Core service update restarts the service, so protection is not in place while the service restarts.
- On Windows, forwarded traffic (Internet Connection Sharing, NAT for WSL2 or Hyper-V guests) is not covered by the kill switch ([bimeron-core#164](https://github.com/bimeron/bimeron-core/issues/164)).
- mDNS (UDP 5353) is allowed directly, outside the tunnel.
- When the outbound path is blocked rather than closed, a lost connection is detected after about 80 seconds.
- The Bimeron Core service belongs to the user who installed it. On a computer with several accounts, another user cannot connect with it, and Repair from that account stops with "Service ownership could not be established" without changing anything. To move the service to another account, the owner or an administrator removes it (see Uninstalling, keep the Desktop package installed), and the other user then selects Fix connection to install it for themselves.
- Desktop 0.3.0 with an in-app Core update to `v0.4.0-beta.2`: running Repair there restores the Core bundled with Desktop 0.3.0 (`v0.4.0-beta.1`). Install the update again afterwards, or upgrade to Desktop 0.3.1. Reinstalling Desktop 0.3.0 does not downgrade the service.
- The Windows ARM64 package was built and its contents verified (the bundled Core and the repair helper equal the published Core `v0.4.0-beta.2` ARM64 executable, Wintun is the ARM64 build, and every executable targets ARM64), but it has not yet been run on a device. Acceptance on Windows 11 ARM64 follows. The Linux arm64 package is built and package-verified but has not been tested on ARM64 hardware.

## Licenses

Bimeron Desktop is MIT-licensed. Windows packages include Qt 6.8.3 under the LGPL-3.0; see `QT-NOTICE.txt` in the installation directory.

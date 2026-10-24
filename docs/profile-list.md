# Profile list labels

Each profile row shows its name followed by the upstream connection protocol, a right arrow, and the local interface mode. `SSConf` is identified by `⟳` beside Shadowsocks and a dynamic-profile tooltip. A legacy `Key` uses its URI scheme. `File` shows only the interface while disconnected; its protocol may be shown only while connected, using the key file read for that connection (including a UTF-8 BOM). Unknown sources show “Unknown protocol” in red without the arrow or interface. The interface is “System VPN” by default and “Local SOCKS5” for `Mode = proxy`.

Unknown profiles cannot start from the list, button, tray, or automatic connection. Their visual editor allows changing the key source and value. Recognized sources lock the source selector. A valid VLESS URI exposes its settings in a read-only VLESS tab, and saving retains the URI.

Acceptance: imported and edited profiles show the appropriate labels; file sources hide uncertain protocol details after disconnect; invalid sources cannot start and can be repaired in the visual editor; a VLESS URI's details, including gRPC `mode=multi`, are viewable without converting it to explicit settings.

# Add profile dialog sizing

The Add profile dialog fits its visible controls and cannot be resized by dragging. A valid key in the clipboard shows the paste button; removing it hides the button and restores the shorter height. A valid key in the input shows the profile name controls; clearing or invalidating it hides them and restores the height for the current clipboard state. These transitions work in either order without leaving blank space.

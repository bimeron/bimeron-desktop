# Bimeron brand assets

`bimeron.png` is the 64 px image from [`assets/icons/bimeron.ico`](../icons/bimeron.ico),
shown at 32 px in the repository README heading so it stays sharp on high-density displays.
It is byte-identical to `assets/brand/bimeron.png` in Bimeron Core.

This directory is not part of the build: CMake embeds only files under `assets/icons/`.

| Field | Value |
| --- | --- |
| Source | `assets/icons/bimeron.ico`, last changed in `67a05d1` |
| ICO SHA-256 | `5824b02b014fff8c21e6e3c78bc3353d6e2934cd45d539b09ba6ff1c18142866` |
| PNG SHA-256 | `c88f022db262c6a2e8b7f6524f2ab19bf486e41683b09b8dafb929f813cd896a` |

## Updating the image

The ICO stores each size as an embedded PNG, so the image is copied without re-encoding.
After changing `bimeron.ico`, run from the repository root:

```bash
python3 - <<'EOF'
import pathlib, struct
ico = pathlib.Path("assets/icons/bimeron.ico").read_bytes()
for i in range(struct.unpack_from("<H", ico, 4)[0]):
    width, _, _, _, _, _, size, offset = struct.unpack_from("<BBBBHHII", ico, 6 + 16 * i)
    if width == 64:
        pathlib.Path("assets/brand/bimeron.png").write_bytes(ico[offset:offset + size])
EOF
```

Then update the hashes above and copy the same file to Bimeron Core.

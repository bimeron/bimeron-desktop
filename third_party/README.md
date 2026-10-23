# Vendored update verification code

- Monocypher 4.0.2 (`monocypher.c/.h` and `monocypher-ed25519.c/.h`) provides portable Ed25519 verification. Source: `https://github.com/LoupVaillant/Monocypher/tree/4.0.2/src`. License: BSD-2-Clause or CC0-1.0; see `monocypher/LICENSE`.
- miniz 3.0.2 provides bounded in-memory ZIP reading. Source: `https://github.com/richgel999/miniz/tree/3.0.2`. License: MIT; see `miniz/LICENSE`. `miniz_export.h` supplies the empty export macro required by this static build.

Both are compiled into the desktop executable. The update verifier uses only Monocypher's Ed25519 check and miniz's ZIP reader. Their licenses are copied into MSI and DEB payloads.

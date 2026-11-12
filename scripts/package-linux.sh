#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
cmake -DBIMERON_CORE_PLATFORM=linux -P scripts/fetch-core.cmake
cmake -P scripts/fetch-geodata.cmake
mkdir -p dist /tmp/bimeron-desktop-packages
rm -f /tmp/bimeron-desktop-packages/bimeron-desktop-*.deb

for preset in linux-amd64-release linux-arm64-release; do
    cmake --preset "$preset" -DBIMERON_REQUIRE_CORE=ON -DBIMERON_CORE_ROOT="$root/.tools/core" \
        -DBIMERON_REQUIRE_GEODATA=ON -DBIMERON_GEODATA_ROOT="$root/.tools/geodata"
    cmake --build --preset "$preset"
    rm -rf -- "$root/stage/${preset%-release}"
    cmake --install "build/${preset%-release}"
    cpack --preset "$preset"
done

rm -f dist/bimeron-desktop-*-linux-*.deb
cp /tmp/bimeron-desktop-packages/bimeron-desktop-*.deb dist/

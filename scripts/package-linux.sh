#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
cmake -DBIMERON_CORE_PLATFORM=linux -P scripts/fetch-core.cmake
cmake -P scripts/fetch-geodata.cmake
mkdir -p dist /tmp/bimeron-desktop-packages
rm -f /tmp/bimeron-desktop-packages/bimeron-desktop-*.deb

for preset in linux-amd64-release linux-arm64-release; do
    target=${preset%-release}
    broker="$root/.tools/core/$target/bimeron"
    digest=$(sha256sum "$broker" | cut -d " " -f 1)
    cmake --preset "$preset" -DBIMERON_REPAIR_BROKER="$broker" -DBIMERON_REPAIR_BROKER_SHA256="$digest" -DBIMERON_REQUIRE_CORE=ON -DBIMERON_CORE_ROOT="$root/.tools/core" \
        -DBIMERON_REQUIRE_GEODATA=ON -DBIMERON_GEODATA_ROOT="$root/.tools/geodata"
    cmake --build --preset "$preset"
    cmake -DBIMERON_LINUX_EXECUTABLE="$root/build/$target/bimeron-desktop" -P scripts/verify-linux-abi.cmake
    rm -rf -- "$root/stage/${preset%-release}"
    cmake --install "build/${preset%-release}"
    cmake -DBIMERON_REPAIR_PAYLOAD="$root/stage/$target/libexec/bimeron/repair/bimeron-repair" \
        -DBIMERON_REPAIR_TARGET="$target" -P scripts/verify-repair-payload.cmake
    cpack --preset "$preset"
done

rm -f dist/bimeron-desktop-*-linux-*.deb
cp /tmp/bimeron-desktop-packages/bimeron-desktop-*.deb dist/

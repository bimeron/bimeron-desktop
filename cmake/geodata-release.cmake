# Third-party country (GeoIP) and site (GeoSite) databases shipped as the default
# selection. Bimeron does not author them; they are Loyalsoldier's v2ray-rules-dat
# release assets (GPL-3.0). Update the tag and both SHA-256 values together.
set(BIMERON_GEODATA_TAG "202611112204")
set(BIMERON_GEODATA_REPOSITORY_URL "https://github.com/Loyalsoldier/v2ray-rules-dat")
set(BIMERON_GEODATA_SHA256_geoip "90b78ab861c1cdc3f871f305f9ceedfb8d6d6329e3a40295e6773e057297384c")
set(BIMERON_GEODATA_SHA256_geosite "8c3cd1740067622985b0182d63ee1ab78f433fa42884b845c6b652a23c4a6318")
set(BIMERON_GEODATA_KINDS geoip geosite)
set(BIMERON_GEODATA_ROOT_DEFAULT "${CMAKE_CURRENT_LIST_DIR}/../.tools/geodata")
# Upstream LICENSE at commit 9a1fbf47e0a1f673eac87ebe90178db5ecd7d21c (Git blob f288702d).
set(BIMERON_GEODATA_LICENSE "${CMAKE_CURRENT_LIST_DIR}/../third_party/loyalsoldier-v2ray-rules-dat/LICENSE")

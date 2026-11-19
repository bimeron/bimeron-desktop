cmake_minimum_required(VERSION 3.21)
project(core-update-keys-fixture NONE)

# Configured by tests/CoreUpdateKeysTest.cmake only; never part of the product build.
include("${BIMERON_SOURCE_DIR}/cmake/core-update-keys.cmake")
if(FIXTURE_LEGACY)
    # The single-key cache entry exactly as Desktop 0.3.0 and earlier declared it.
    set(BIMERON_CORE_UPDATE_PUBLIC_KEY "bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=" CACHE STRING
        "Base64 Ed25519 public key for signed core updates")
else()
    # The pinned list stands in for cmake/core-update-keys.cmake of a source tree.
    include("${FIXTURE_PINNED_FILE}")
    bimeron_resolve_core_update_keys(FIXTURE_PINNED)
    file(WRITE "${CMAKE_BINARY_DIR}/result.cmake"
        "set(RESULT_KEYS \"${BIMERON_CORE_UPDATE_TRUSTED_KEYS}\")\n"
        "set(RESULT_IDS \"${BIMERON_CORE_UPDATE_TRUSTED_KEY_IDS}\")\n"
        "set(RESULT_PINNED ${BIMERON_CORE_UPDATE_KEYS_PINNED})\n")
endif()

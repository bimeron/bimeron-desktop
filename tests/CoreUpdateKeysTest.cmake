# Configure-time checks for cmake/core-update-keys.cmake.
#
# Driver: cmake -DBIMERON_SOURCE_DIR=<src> -DFIXTURE_ROOT=<dir> -DFIXTURE_GENERATOR=<gen>
#               [-DFIXTURE_PLATFORM=<arch>] -P tests/CoreUpdateKeysTest.cmake
# The driver re-runs this script with -DCASE=<name> for each list that must be
# rejected, and configures tests/core-update-keys-fixture to exercise real
# CMake caches, including a build directory created by the single-key releases.
# Only public keys appear here.
cmake_minimum_required(VERSION 3.21)
include("${BIMERON_SOURCE_DIR}/cmake/core-update-keys.cmake")

# The production key and two independent vectors with known key IDs.
set(K1 "bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=")
set(K1_ID "7370a2d8cf890e5e")
# Public key of the Core release/applemanifest test vector (KeyID 139e3940e64b5491).
set(K2 "O2onvM62pC1io6jQKm8Nc2UyFXcd4kOmOsBIoYtZ2ik=")
set(K2_ID "139e3940e64b5491")
# 32 zero bytes; SHA-256 66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925.
string(REPEAT "A" 43 K0)
string(APPEND K0 "=")
set(K0_ID "66687aadf862bd77")

function(fail text)
    message(FATAL_ERROR "core-update-keys-test: ${text}")
endfunction()

if(DEFINED CASE)
    set(flag "")
    string(REPLACE "+" "-" urlsafe "${K1}")
    string(SUBSTRING "${K1}" 0 42 truncated)
    string(SUBSTRING "${K1}" 0 43 unpadded)
    string(REPEAT "A" 44 too_long)
    if(CASE STREQUAL "mismatch")
        set(entries "${K2_ID}:${K1}")
    elseif(CASE STREQUAL "mismatch-second")
        set(entries "${K1_ID}:${K1}" "${K1_ID}:${K2}")
    elseif(CASE STREQUAL "mismatch-third")
        # Two successors with their key IDs swapped.
        set(entries "${K1_ID}:${K1}" "${K2_ID}:${K2}" "${K2_ID}:${K0}")
    elseif(CASE STREQUAL "duplicate")
        set(entries "${K1}" "${K2}" "${K1}")
    elseif(CASE STREQUAL "duplicate-successor")
        set(entries "${K1_ID}:${K1}" "${K2_ID}:${K2}" "${K2_ID}:${K2}")
    elseif(CASE STREQUAL "duplicate-with-id")
        set(entries "${K1_ID}:${K1}" "${K1}")
    elseif(CASE STREQUAL "truncated")
        set(entries "${truncated}=")
    elseif(CASE STREQUAL "unpadded")
        set(entries "${unpadded}")
    elseif(CASE STREQUAL "too-long")
        set(entries "${too_long}")
    elseif(CASE STREQUAL "noncanonical")
        set(entries "bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF1=")
    elseif(CASE STREQUAL "urlsafe")
        set(entries "${urlsafe}")
    elseif(CASE STREQUAL "whitespace")
        set(entries " ${K1}")
    elseif(CASE STREQUAL "empty-entry")
        set(entries "${K1};;${K2}")
    elseif(CASE STREQUAL "uppercase-id")
        set(entries "7370A2D8CF890E5E:${K1}")
    elseif(CASE STREQUAL "short-id")
        set(entries "7370a2d8:${K1}")
    elseif(CASE STREQUAL "empty-id")
        set(entries ":${K1}")
    elseif(CASE STREQUAL "pinned-without-id")
        set(entries "${K1_ID}:${K1}" "${K2}")
        set(flag REQUIRE_KEY_IDS)
    else()
        fail("unknown case ${CASE}")
    endif()
    bimeron_core_update_parse_keys(keys ids entries ${flag})
    message(STATUS "case ${CASE} was accepted: ${keys}")
    return()
endif()

foreach(name K1 K2 K0)
    bimeron_core_update_key_id(id "${${name}}")
    if(NOT id STREQUAL "${${name}_ID}")
        fail("key ID of ${${name}} is ${id}, expected ${${name}_ID}")
    endif()
endforeach()

set(entries "${K1}" "${K2_ID}:${K2}" "${K0}")
bimeron_core_update_parse_keys(keys ids entries)
if(NOT "${keys}" STREQUAL "${K1};${K2};${K0}" OR NOT "${ids}" STREQUAL "${K1_ID};${K2_ID};${K0_ID}")
    fail("valid list parsed as '${keys}' with IDs '${ids}'")
endif()
# The rotation shape: the current key and two successors, each with its key ID.
set(three_pinned "${K1_ID}:${K1}" "${K2_ID}:${K2}" "${K0_ID}:${K0}")
bimeron_core_update_parse_keys(keys ids three_pinned REQUIRE_KEY_IDS)
if(NOT "${keys}" STREQUAL "${K1};${K2};${K0}" OR NOT "${ids}" STREQUAL "${K1_ID};${K2_ID};${K0_ID}")
    fail("three pinned keys parsed as '${keys}' with IDs '${ids}'")
endif()
set(entries "")
bimeron_core_update_parse_keys(keys ids entries)
if(NOT "${keys}" STREQUAL "" OR NOT "${ids}" STREQUAL "")
    fail("empty list parsed as '${keys}'")
endif()
bimeron_core_update_parse_keys(keys ids BIMERON_CORE_UPDATE_PINNED_KEYS REQUIRE_KEY_IDS)
list(GET keys 0 first_key)
list(GET ids 0 first_id)
if(NOT first_key STREQUAL K1 OR NOT first_id STREQUAL K1_ID)
    fail("the current production key is not the first pinned key")
endif()
if(NOT BIMERON_CORE_UPDATE_LEGACY_DEFAULT_KEY STREQUAL K1)
    fail("the legacy default is not the former single production key")
endif()

function(normalize out text)
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(expect_rejected case expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DBIMERON_SOURCE_DIR=${BIMERON_SOURCE_DIR}" "-DCASE=${case}"
                            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    normalize(output "${output}")
    if(result EQUAL 0)
        fail("case ${case} was accepted: ${output}")
    endif()
    string(FIND "${output}" "${expected}" found)
    if(found LESS 0)
        fail("case ${case} failed without '${expected}': ${output}")
    endif()
endfunction()

expect_rejected(mismatch "entry 1 declares key ID ${K2_ID}, but its public key ${K1} has key ID ${K1_ID}")
expect_rejected(mismatch-second "entry 2 declares key ID ${K1_ID}, but its public key ${K2} has key ID ${K2_ID}")
expect_rejected(mismatch-third "entry 3 declares key ID ${K2_ID}, but its public key ${K0} has key ID ${K0_ID}")
expect_rejected(duplicate "entry 3 duplicates entry 1 (key ID ${K1_ID})")
expect_rejected(duplicate-successor "entry 3 duplicates entry 2 (key ID ${K2_ID})")
expect_rejected(duplicate-with-id "entry 2 duplicates entry 1 (key ID ${K1_ID})")
foreach(case truncated unpadded too-long noncanonical urlsafe whitespace)
    expect_rejected(${case} "entry 1 is not a standard-base64 32-byte Ed25519 public key")
endforeach()
expect_rejected(empty-entry "entry 2 is not a standard-base64 32-byte Ed25519 public key: ''")
expect_rejected(uppercase-id "entry 1 has malformed key ID '7370A2D8CF890E5E'")
expect_rejected(short-id "entry 1 has malformed key ID '7370a2d8'")
expect_rejected(empty-id "entry 1 has malformed key ID ''")
expect_rejected(pinned-without-id "entry 2 must have the form <key_id>:<public key>")

# Real configure runs. pinned.cmake plays the role of the source tree's pinned list.
set(pinned_file "${FIXTURE_ROOT}/pinned.cmake")
file(REMOVE_RECURSE "${FIXTURE_ROOT}")
file(MAKE_DIRECTORY "${FIXTURE_ROOT}")

function(write_pinned)
    set(content "set(FIXTURE_PINNED")
    foreach(entry IN LISTS ARGN)
        string(APPEND content " \"${entry}\"")
    endforeach()
    file(WRITE "${pinned_file}" "${content})\n")
endfunction()

# configure_fixture(<dir> SUCCESS|FAILURE <expected text or ""> [cmake arguments...])
function(configure_fixture name outcome expected)
    set(dir "${FIXTURE_ROOT}/${name}")
    file(REMOVE "${dir}/result.cmake")
    set(command "${CMAKE_COMMAND}" -S "${BIMERON_SOURCE_DIR}/tests/core-update-keys-fixture" -B "${dir}"
        -G "${FIXTURE_GENERATOR}")
    if(NOT "${FIXTURE_PLATFORM}" STREQUAL "")
        list(APPEND command -A "${FIXTURE_PLATFORM}")
    endif()
    list(APPEND command "-DBIMERON_SOURCE_DIR=${BIMERON_SOURCE_DIR}" "-DFIXTURE_PINNED_FILE=${pinned_file}" ${ARGN})
    execute_process(COMMAND ${command} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    normalize(output "${output}")
    if(outcome STREQUAL "SUCCESS" AND NOT result EQUAL 0)
        fail("configure ${name} ${ARGN} failed: ${output}")
    elseif(outcome STREQUAL "FAILURE" AND result EQUAL 0)
        fail("configure ${name} ${ARGN} succeeded: ${output}")
    endif()
    if(NOT expected STREQUAL "")
        string(FIND "${output}" "${expected}" found)
        if(found LESS 0)
            fail("configure ${name} ${ARGN} lacks '${expected}': ${output}")
        endif()
    endif()
endfunction()

function(expect_result name keys ids pinned)
    set(RESULT_KEYS "<missing>")
    include("${FIXTURE_ROOT}/${name}/result.cmake" OPTIONAL)
    if(NOT "${RESULT_KEYS}" STREQUAL "${keys}" OR NOT "${RESULT_IDS}" STREQUAL "${ids}" OR
       NOT "${RESULT_PINNED}" STREQUAL "${pinned}")
        fail("${name} trusts '${RESULT_KEYS}' (IDs '${RESULT_IDS}', pinned ${RESULT_PINNED}), expected "
            "'${keys}' (IDs '${ids}', pinned ${pinned})")
    endif()
endfunction()

# expect_cache(<dir> <entry name> PRESENT|ABSENT)
function(expect_cache name entry state)
    file(STRINGS "${FIXTURE_ROOT}/${name}/CMakeCache.txt" lines REGEX "^${entry}:")
    if(state STREQUAL "ABSENT" AND lines)
        fail("${name} still caches ${lines}")
    elseif(state STREQUAL "PRESENT" AND NOT lines)
        fail("${name} does not cache ${entry}")
    endif()
endfunction()

set(rotation_keys "${K1};${K2};${K0}")
set(rotation_ids "${K1_ID};${K2_ID};${K0_ID}")

# A build directory from a single-key release caches the former default. It
# must follow the pinned list, including two keys added at once, and drop the entry.
write_pinned("${K1_ID}:${K1}")
configure_fixture(stale-default SUCCESS "" -DFIXTURE_LEGACY=ON)
expect_cache(stale-default BIMERON_CORE_UPDATE_PUBLIC_KEY PRESENT)
configure_fixture(stale-default SUCCESS "Removed the cached former default BIMERON_CORE_UPDATE_PUBLIC_KEY"
    -DFIXTURE_LEGACY=OFF)
expect_result(stale-default "${K1}" "${K1_ID}" TRUE)
expect_cache(stale-default BIMERON_CORE_UPDATE_PUBLIC_KEY ABSENT)
expect_cache(stale-default BIMERON_CORE_UPDATE_PUBLIC_KEYS ABSENT)
write_pinned("${K1_ID}:${K1}" "${K2_ID}:${K2}" "${K0_ID}:${K0}")
configure_fixture(stale-default SUCCESS "Trusted Core update key IDs (pinned): ${K1_ID}, ${K2_ID}, ${K0_ID}"
    -DFIXTURE_LEGACY=OFF)
expect_result(stale-default "${rotation_keys}" "${rotation_ids}" TRUE)
# Retiring the old key later takes effect in the same build directory too.
write_pinned("${K2_ID}:${K2}" "${K0_ID}:${K0}")
configure_fixture(stale-default SUCCESS "" -DFIXTURE_LEGACY=OFF)
expect_result(stale-default "${K2};${K0}" "${K2_ID};${K0_ID}" TRUE)
write_pinned("${K1_ID}:${K1}" "${K2_ID}:${K2}" "${K0_ID}:${K0}")

# A cached non-default single key fails instead of replacing the pinned list.
configure_fixture(stale-custom SUCCESS "" -DFIXTURE_LEGACY=ON "-DBIMERON_CORE_UPDATE_PUBLIC_KEY=${K2}")
configure_fixture(stale-custom FAILURE "Reconfigure with -U BIMERON_CORE_UPDATE_PUBLIC_KEY" -DFIXTURE_LEGACY=OFF)
expect_cache(stale-custom BIMERON_CORE_UPDATE_PUBLIC_KEY PRESENT)
configure_fixture(stale-custom SUCCESS "" -DFIXTURE_LEGACY=OFF -UBIMERON_CORE_UPDATE_PUBLIC_KEY)
expect_result(stale-custom "${rotation_keys}" "${rotation_ids}" TRUE)

# A cached empty single key (signed updates disabled) also needs an explicit decision.
configure_fixture(stale-empty SUCCESS "" -DFIXTURE_LEGACY=ON "-DBIMERON_CORE_UPDATE_PUBLIC_KEY=")
configure_fixture(stale-empty FAILURE "BIMERON_CORE_UPDATE_PUBLIC_KEY is no longer supported" -DFIXTURE_LEGACY=OFF)

# An explicit list override is cached until removed; an empty one disables updates.
file(WRITE "${FIXTURE_ROOT}/override.cmake"
    "set(BIMERON_CORE_UPDATE_PUBLIC_KEYS \"${K2};${K0_ID}:${K0}\" CACHE STRING \"\")\n")
configure_fixture(override SUCCESS "BIMERON_CORE_UPDATE_PUBLIC_KEYS overrides the pinned Core update keys"
    -DFIXTURE_LEGACY=OFF -C "${FIXTURE_ROOT}/override.cmake")
expect_result(override "${K2};${K0}" "${K2_ID};${K0_ID}" FALSE)
configure_fixture(override SUCCESS "" -DFIXTURE_LEGACY=OFF)
expect_result(override "${K2};${K0}" "${K2_ID};${K0_ID}" FALSE)
configure_fixture(override SUCCESS "disables signed Core updates" -DFIXTURE_LEGACY=OFF
    "-DBIMERON_CORE_UPDATE_PUBLIC_KEYS=")
expect_result(override "" "" FALSE)
configure_fixture(override SUCCESS "" -DFIXTURE_LEGACY=OFF -UBIMERON_CORE_UPDATE_PUBLIC_KEYS)
expect_result(override "${rotation_keys}" "${rotation_ids}" TRUE)
expect_cache(override BIMERON_CORE_UPDATE_PUBLIC_KEYS ABSENT)
file(WRITE "${FIXTURE_ROOT}/malformed.cmake"
    "set(BIMERON_CORE_UPDATE_PUBLIC_KEYS \"${K1};${K1}\" CACHE STRING \"\")\n")
configure_fixture(override FAILURE "BIMERON_CORE_UPDATE_PUBLIC_KEYS entry 2 duplicates entry 1"
    -DFIXTURE_LEGACY=OFF -C "${FIXTURE_ROOT}/malformed.cmake")

message(STATUS "core-update-keys-test passed")

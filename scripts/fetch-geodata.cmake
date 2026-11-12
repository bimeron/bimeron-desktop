cmake_minimum_required(VERSION 3.21)
# Downloads and verifies the pinned Loyalsoldier databases into
# <root>/<tag>/ together with release.json and SOURCE.txt. A verified cache is reused
# offline; a file that fails the pinned SHA-256 is downloaded again or rejected.
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/geodata-release.cmake")

if(NOT DEFINED BIMERON_GEODATA_ROOT)
    set(BIMERON_GEODATA_ROOT "${BIMERON_GEODATA_ROOT_DEFAULT}")
endif()
set(directory "${BIMERON_GEODATA_ROOT}/${BIMERON_GEODATA_TAG}")
file(MAKE_DIRECTORY "${directory}")
foreach(kind IN LISTS BIMERON_GEODATA_KINDS)
    set(file "${directory}/${kind}.dat")
    set(checksum "${BIMERON_GEODATA_SHA256_${kind}}")
    unset(actual)
    if(EXISTS "${file}")
        file(SHA256 "${file}" actual)
    endif()
    if(NOT actual STREQUAL checksum)
        file(DOWNLOAD
            "${BIMERON_GEODATA_REPOSITORY_URL}/releases/download/${BIMERON_GEODATA_TAG}/${kind}.dat"
            "${file}"
            EXPECTED_HASH "SHA256=${checksum}"
            STATUS status)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${file}")
            message(FATAL_ERROR "Database download failed for ${kind}.dat: ${status}")
        endif()
    endif()
endforeach()
file(WRITE "${directory}/release.json" "{\"tag\":\"${BIMERON_GEODATA_TAG}\"}\n")
file(WRITE "${directory}/SOURCE.txt"
    "Country (geoip.dat) and site (geosite.dat) databases\n"
    "Author: Loyalsoldier, ${BIMERON_GEODATA_REPOSITORY_URL}\n"
    "Release: ${BIMERON_GEODATA_TAG}\n"
    "Source: ${BIMERON_GEODATA_REPOSITORY_URL}/releases/tag/${BIMERON_GEODATA_TAG}\n"
    "License: GPL-3.0; see LICENSE.txt in this directory.\n"
    "Bimeron ships these files unmodified as the default selection and does not author them.\n")
message(STATUS "Databases ${BIMERON_GEODATA_TAG} ready")

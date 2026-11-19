# Trusted Ed25519 public keys for signed Bimeron Core update manifests.
#
# Each pinned entry is "<key_id>:<standard-base64 public key>". The key ID is
# never trusted on its own: configuration derives it from the key exactly as
# Core does (lowercase hex of the first 8 bytes of SHA-256 over the raw 32-byte
# public key) and fails when the recorded ID differs. Only public keys belong
# here; private keys never enter this repository.
#
# During a signing key rotation the list holds the current key and one or more
# successors, one entry each; see docs/core-updates.md for the procedure.
set(BIMERON_CORE_UPDATE_PINNED_KEYS
    # Software key: signed every Core release up to and including v0.4.0-beta.2; signs nothing new.
    "7370a2d8cf890e5e:bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0="
    # YubiKey A, serial 22337246, OpenPGP signature slot, Ed25519 generated on-card:
    # primary signer from Core v0.4.0-beta.3 on.
    "e6fb20d55656904a:lRcnhdj4yU4TrinOa4U177FG7wrwUPv/v9zxA/WiJMI="
    # YubiKey B, serial 19344964, OpenPGP signature slot, Ed25519 generated on-card:
    # backup signer, used only when YubiKey A is unavailable.
    "095d295f5fe9a4e2:6K8uY/1qIogN2j3ZdpEr4PuTqXzH7EseTd6kzHxpIr4=")

# The former single-key cache default. Build directories configured before the
# key list existed cache this value in BIMERON_CORE_UPDATE_PUBLIC_KEY.
set(BIMERON_CORE_UPDATE_LEGACY_DEFAULT_KEY "bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=")

set(_BIMERON_SHA256_K
    0x428a2f98 0x71374491 0xb5c0fbcf 0xe9b5dba5 0x3956c25b 0x59f111f1 0x923f82a4 0xab1c5ed5
    0xd807aa98 0x12835b01 0x243185be 0x550c7dc3 0x72be5d74 0x80deb1fe 0x9bdc06a7 0xc19bf174
    0xe49b69c1 0xefbe4786 0x0fc19dc6 0x240ca1cc 0x2de92c6f 0x4a7484aa 0x5cb0a9dc 0x76f988da
    0x983e5152 0xa831c66d 0xb00327c8 0xbf597fc7 0xc6e00bf3 0xd5a79147 0x06ca6351 0x14292967
    0x27b70a85 0x2e1b2138 0x4d2c6dfc 0x53380d13 0x650a7354 0x766a0abb 0x81c2c92e 0x92722c85
    0xa2bfe8a1 0xa81a664b 0xc24b8b70 0xc76c51a3 0xd192e819 0xd6990624 0xf40e3585 0x106aa070
    0x19a4c116 0x1e376c08 0x2748774c 0x34b0bcb5 0x391c0cb3 0x4ed8aa4a 0x5b9cca4f 0x682e6ff3
    0x748f82ee 0x78a5636f 0x84c87814 0x8cc70208 0x90befffa 0xa4506ceb 0xbef9a3f7 0xc67178f2)

string(REPEAT "[A-Za-z0-9+/]" 42 _BIMERON_BASE64_BODY)
set(_BIMERON_CORE_UPDATE_KEY_PATTERN "^${_BIMERON_BASE64_BODY}[AEIMQUYcgkosw048]=$")
string(REPEAT "[0-9a-f]" 16 _BIMERON_CORE_UPDATE_KEY_ID_PATTERN)
set(_BIMERON_CORE_UPDATE_KEY_ID_PATTERN "^${_BIMERON_CORE_UPDATE_KEY_ID_PATTERN}$")

# True when value is standard base64 of exactly 32 bytes in canonical form
# (43 data characters, zero padding bits and one "=").
function(bimeron_core_update_key_is_valid out value)
    if(value MATCHES "${_BIMERON_CORE_UPDATE_KEY_PATTERN}")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Derives the Core key ID of a validated standard-base64 32-byte public key:
# lowercase hex of the first 8 bytes of SHA-256 over the raw key bytes.
function(bimeron_core_update_key_id out key)
    bimeron_core_update_key_is_valid(valid "${key}")
    if(NOT valid)
        message(FATAL_ERROR "Not a standard-base64 32-byte Ed25519 public key: '${key}'")
    endif()

    # Decode the 43 data characters into 32 bytes.
    set(alphabet "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
    set(bytes)
    set(acc 0)
    set(nbits 0)
    foreach(index RANGE 0 42)
        string(SUBSTRING "${key}" ${index} 1 char)
        string(FIND "${alphabet}" "${char}" value)
        math(EXPR acc "((${acc} << 6) | ${value}) & 0x3FFF")
        math(EXPR nbits "${nbits} + 6")
        if(nbits GREATER_EQUAL 8)
            math(EXPR nbits "${nbits} - 8")
            math(EXPR byte "(${acc} >> ${nbits}) & 0xFF")
            list(APPEND bytes ${byte})
        endif()
    endforeach()
    list(LENGTH bytes count)
    if(NOT count EQUAL 32)
        message(FATAL_ERROR "Internal error: decoded ${count} bytes from '${key}'")
    endif()

    # A 32-byte message is a single padded SHA-256 block: the key words, the
    # 0x80 terminator, zeros and the 256-bit message length.
    set(w)
    foreach(word RANGE 0 7)
        math(EXPR i0 "${word} * 4")
        math(EXPR i1 "${i0} + 1")
        math(EXPR i2 "${i0} + 2")
        math(EXPR i3 "${i0} + 3")
        list(GET bytes ${i0} b0)
        list(GET bytes ${i1} b1)
        list(GET bytes ${i2} b2)
        list(GET bytes ${i3} b3)
        math(EXPR value "(${b0} << 24) | (${b1} << 16) | (${b2} << 8) | ${b3}")
        list(APPEND w ${value})
    endforeach()
    list(APPEND w 2147483648 0 0 0 0 0 0 256)
    foreach(t RANGE 16 63)
        math(EXPR i15 "${t} - 15")
        math(EXPR i2 "${t} - 2")
        math(EXPR i16 "${t} - 16")
        math(EXPR i7 "${t} - 7")
        list(GET w ${i15} x15)
        list(GET w ${i2} x2)
        list(GET w ${i16} x16)
        list(GET w ${i7} x7)
        math(EXPR s0 "((${x15} >> 7) | ((${x15} << 25) & 0xFFFFFFFF)) ^ ((${x15} >> 18) | ((${x15} << 14) & 0xFFFFFFFF)) ^ (${x15} >> 3)")
        math(EXPR s1 "((${x2} >> 17) | ((${x2} << 15) & 0xFFFFFFFF)) ^ ((${x2} >> 19) | ((${x2} << 13) & 0xFFFFFFFF)) ^ (${x2} >> 10)")
        math(EXPR value "(${x16} + ${s0} + ${x7} + ${s1}) & 0xFFFFFFFF")
        list(APPEND w ${value})
    endforeach()

    set(h0 1779033703)  # 0x6a09e667
    set(h1 3144134277)  # 0xbb67ae85
    set(a ${h0})
    set(b ${h1})
    set(c 1013904242)   # 0x3c6ef372
    set(d 2773480762)   # 0xa54ff53a
    set(e 1359893119)   # 0x510e527f
    set(f 2600822924)   # 0x9b05688c
    set(g 528734635)    # 0x1f83d9ab
    set(h 1541459225)   # 0x5be0cd19
    foreach(t RANGE 0 63)
        list(GET _BIMERON_SHA256_K ${t} k)
        list(GET w ${t} wt)
        math(EXPR sum1 "((${e} >> 6) | ((${e} << 26) & 0xFFFFFFFF)) ^ ((${e} >> 11) | ((${e} << 21) & 0xFFFFFFFF)) ^ ((${e} >> 25) | ((${e} << 7) & 0xFFFFFFFF))")
        math(EXPR choose "(${e} & ${f}) ^ ((${e} ^ 0xFFFFFFFF) & ${g})")
        math(EXPR temp1 "(${h} + ${sum1} + ${choose} + ${k} + ${wt}) & 0xFFFFFFFF")
        math(EXPR sum0 "((${a} >> 2) | ((${a} << 30) & 0xFFFFFFFF)) ^ ((${a} >> 13) | ((${a} << 19) & 0xFFFFFFFF)) ^ ((${a} >> 22) | ((${a} << 10) & 0xFFFFFFFF))")
        math(EXPR majority "(${a} & ${b}) ^ (${a} & ${c}) ^ (${b} & ${c})")
        set(h ${g})
        set(g ${f})
        set(f ${e})
        math(EXPR e "(${d} + ${temp1}) & 0xFFFFFFFF")
        set(d ${c})
        set(c ${b})
        set(b ${a})
        math(EXPR a "(${temp1} + ${sum0} + ${majority}) & 0xFFFFFFFF")
    endforeach()

    # The key ID needs only the first two digest words.
    math(EXPR word0 "(${h0} + ${a}) & 0xFFFFFFFF" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR word1 "(${h1} + ${b}) & 0xFFFFFFFF" OUTPUT_FORMAT HEXADECIMAL)
    set(id "")
    foreach(word IN ITEMS ${word0} ${word1})
        string(SUBSTRING "${word}" 2 -1 word)
        string(TOLOWER "${word}" word)
        string(LENGTH "${word}" length)
        while(length LESS 8)
            string(PREPEND word "0")
            math(EXPR length "${length} + 1")
        endwhile()
        string(APPEND id "${word}")
    endforeach()
    set(${out} "${id}" PARENT_SCOPE)
endfunction()

# Validates the key list stored in the variable <list_var>. Entries are
# "[<key_id>:]<base64 key>"; with REQUIRE_KEY_IDS every entry must carry its
# key ID. Sets <out_keys> and <out_ids> to the keys and their derived IDs, in
# order. Malformed or empty entries, key ID mismatches, duplicate keys and
# duplicate key IDs fail configuration.
function(bimeron_core_update_parse_keys out_keys out_ids list_var)
    cmake_parse_arguments(PARSE_ARGV 3 _bk "REQUIRE_KEY_IDS" "" "")
    set(_bk_keys)
    set(_bk_ids)
    set(_bk_position 0)
    # IN LISTS also visits empty elements, so "a;;b" is rejected.
    foreach(_bk_entry IN LISTS ${list_var})
        math(EXPR _bk_position "${_bk_position} + 1")
        set(_bk_expected "")
        set(_bk_key "${_bk_entry}")
        if(_bk_entry MATCHES "^([^:]*):(.*)$")
            set(_bk_expected "${CMAKE_MATCH_1}")
            set(_bk_key "${CMAKE_MATCH_2}")
            if(NOT _bk_expected MATCHES "${_BIMERON_CORE_UPDATE_KEY_ID_PATTERN}")
                message(FATAL_ERROR "${list_var} entry ${_bk_position} has malformed key ID "
                    "'${_bk_expected}'; expected 16 lowercase hexadecimal characters before ':'")
            endif()
        elseif(_bk_REQUIRE_KEY_IDS)
            message(FATAL_ERROR "${list_var} entry ${_bk_position} must have the form <key_id>:<public key>")
        endif()
        bimeron_core_update_key_is_valid(_bk_valid "${_bk_key}")
        if(NOT _bk_valid)
            message(FATAL_ERROR "${list_var} entry ${_bk_position} is not a standard-base64 32-byte "
                "Ed25519 public key: '${_bk_key}'")
        endif()
        bimeron_core_update_key_id(_bk_id "${_bk_key}")
        if(NOT _bk_expected STREQUAL "" AND NOT _bk_expected STREQUAL _bk_id)
            message(FATAL_ERROR "${list_var} entry ${_bk_position} declares key ID ${_bk_expected}, "
                "but its public key ${_bk_key} has key ID ${_bk_id}")
        endif()
        # Canonical base64 makes string equality byte equality.
        list(FIND _bk_keys "${_bk_key}" _bk_duplicate)
        if(_bk_duplicate GREATER_EQUAL 0)
            math(EXPR _bk_duplicate "${_bk_duplicate} + 1")
            message(FATAL_ERROR "${list_var} entry ${_bk_position} duplicates entry ${_bk_duplicate} "
                "(key ID ${_bk_id})")
        endif()
        list(FIND _bk_ids "${_bk_id}" _bk_duplicate)
        if(_bk_duplicate GREATER_EQUAL 0)
            math(EXPR _bk_duplicate "${_bk_duplicate} + 1")
            message(FATAL_ERROR "${list_var} entries ${_bk_duplicate} and ${_bk_position} share key ID ${_bk_id}")
        endif()
        list(APPEND _bk_keys "${_bk_key}")
        list(APPEND _bk_ids "${_bk_id}")
    endforeach()
    set(${out_keys} "${_bk_keys}" PARENT_SCOPE)
    set(${out_ids} "${_bk_ids}" PARENT_SCOPE)
endfunction()

# Resolves the keys trusted by this build from the pinned list stored in
# <pinned_var> and the optional BIMERON_CORE_UPDATE_PUBLIC_KEYS override.
#
# Cache behaviour: the pinned list is never written to the CMake cache, so a
# build directory always follows the pinned list of its source tree.
# BIMERON_CORE_UPDATE_PUBLIC_KEYS exists only when given explicitly (-D); it
# then stays cached until removed with -U. An empty override disables signed
# updates. The legacy single-key BIMERON_CORE_UPDATE_PUBLIC_KEY is removed from
# the cache when it holds the former default; any other value fails, so a
# stale single key can never replace the list silently.
#
# Sets BIMERON_CORE_UPDATE_TRUSTED_KEYS, BIMERON_CORE_UPDATE_TRUSTED_KEY_IDS
# and BIMERON_CORE_UPDATE_KEYS_PINNED (TRUE when the keys equal the pinned list).
function(bimeron_resolve_core_update_keys pinned_var)
    bimeron_core_update_parse_keys(_bk_pinned_keys _bk_pinned_ids ${pinned_var} REQUIRE_KEY_IDS)
    if(DEFINED BIMERON_CORE_UPDATE_PUBLIC_KEY)
        if("${BIMERON_CORE_UPDATE_PUBLIC_KEY}" STREQUAL "${BIMERON_CORE_UPDATE_LEGACY_DEFAULT_KEY}")
            unset(BIMERON_CORE_UPDATE_PUBLIC_KEY CACHE)
            message(STATUS "Removed the cached former default BIMERON_CORE_UPDATE_PUBLIC_KEY; "
                "trusted Core update keys now come from the pinned list")
        else()
            message(FATAL_ERROR "BIMERON_CORE_UPDATE_PUBLIC_KEY is no longer supported and holds a "
                "non-default value ('${BIMERON_CORE_UPDATE_PUBLIC_KEY}'). Trusted keys are now the list "
                "BIMERON_CORE_UPDATE_PUBLIC_KEYS. Reconfigure with -U BIMERON_CORE_UPDATE_PUBLIC_KEY to use "
                "the pinned keys, and add -DBIMERON_CORE_UPDATE_PUBLIC_KEYS=<keys> only to keep an explicit "
                "override (an empty value disables signed updates).")
        endif()
    endif()

    if(DEFINED BIMERON_CORE_UPDATE_PUBLIC_KEYS)
        bimeron_core_update_parse_keys(_bk_keys _bk_ids BIMERON_CORE_UPDATE_PUBLIC_KEYS)
    else()
        set(_bk_keys "${_bk_pinned_keys}")
        set(_bk_ids "${_bk_pinned_ids}")
    endif()
    if("${_bk_keys}" STREQUAL "${_bk_pinned_keys}")
        set(_bk_pinned TRUE)
    else()
        set(_bk_pinned FALSE)
    endif()

    list(JOIN _bk_ids ", " _bk_shown)
    if(NOT DEFINED BIMERON_CORE_UPDATE_PUBLIC_KEYS)
        if("${_bk_keys}" STREQUAL "")
            message(STATUS "Signed Core updates are disabled: the pinned key list is empty")
        else()
            message(STATUS "Trusted Core update key IDs (pinned): ${_bk_shown}")
        endif()
    elseif("${_bk_keys}" STREQUAL "")
        message(WARNING "The empty BIMERON_CORE_UPDATE_PUBLIC_KEYS override disables signed Core updates. "
            "Remove it with -U BIMERON_CORE_UPDATE_PUBLIC_KEYS to use the pinned keys.")
    else()
        message(WARNING "BIMERON_CORE_UPDATE_PUBLIC_KEYS overrides the pinned Core update keys; trusted key "
            "IDs: ${_bk_shown}. Remove it with -U BIMERON_CORE_UPDATE_PUBLIC_KEYS to use the pinned keys.")
    endif()
    set(BIMERON_CORE_UPDATE_TRUSTED_KEYS "${_bk_keys}" PARENT_SCOPE)
    set(BIMERON_CORE_UPDATE_TRUSTED_KEY_IDS "${_bk_ids}" PARENT_SCOPE)
    set(BIMERON_CORE_UPDATE_KEYS_PINNED ${_bk_pinned} PARENT_SCOPE)
endfunction()

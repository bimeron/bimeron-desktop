cmake_minimum_required(VERSION 3.21)
project(BimeronDesktop VERSION 0.2.0 LANGUAGES C CXX)

set(BIMERON_PACKAGE_ARCH "unknown" CACHE STRING "Architecture name used in package files")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)
set(BIMERON_CORE_UPDATE_PUBLIC_KEY "bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=" CACHE STRING "Base64 Ed25519 public key for signed core updates")
add_library(core-update STATIC
    src/CoreUpdate.cpp src/CoreUpdate.h
    third_party/monocypher/monocypher.c
    third_party/monocypher/monocypher-ed25519.c
    third_party/miniz/miniz.c
    third_party/miniz/miniz_zip.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_tdef.c)
target_include_directories(core-update PRIVATE third_party/monocypher third_party/miniz)
target_link_libraries(core-update PUBLIC Qt6::Network)

set(BIMERON_DESKTOP_SOURCES
    src/MainWindow.cpp src/MainWindow.h
    src/CoreClient.cpp src/CoreClient.h
    src/NativeIKEv2Transport.cpp src/NativeIKEv2Transport.h
    src/NativeIKEv2Session.cpp src/NativeIKEv2Session.h
    src/DevelopmentCore.cpp src/DevelopmentCore.h
    src/CoreUpdater.cpp src/CoreUpdater.h
    src/TrustedCore.cpp src/TrustedCore.h
    src/ProfileStore.cpp src/ProfileStore.h
    src/ProfileEditorDialog.cpp src/ProfileEditorDialog.h
)
qt_add_executable(bimeron-desktop WIN32 src/main.cpp ${BIMERON_DESKTOP_SOURCES})
file(GLOB_RECURSE BIMERON_ICON_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.xpm")
qt_add_resources(bimeron-desktop "icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    FILES ${BIMERON_ICON_FILES})
qt_add_resources(bimeron-desktop "translations"
    PREFIX "/translations"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/translations"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/translations/ru.json")
if(WIN32)
    target_sources(bimeron-desktop PRIVATE app.rc)
    add_custom_command(TARGET bimeron-desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/monocypher/LICENSE"
            "$<TARGET_FILE_DIR:bimeron-desktop>/MONOCYPHER-LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/LICENSE"
            "$<TARGET_FILE_DIR:bimeron-desktop>/MINIZ-LICENSE.txt"
        VERBATIM)
    add_custom_command(TARGET bimeron-desktop PRE_LINK
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stop-build-app.ps1"
            -Executable "$<TARGET_FILE:bimeron-desktop>"
        VERBATIM)
    option(BIMERON_DEPLOY_QT "Deploy Qt beside the Windows executable" ON)
    if(BIMERON_DEPLOY_QT)
        get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
        add_custom_command(TARGET bimeron-desktop POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --translations ru
                --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:bimeron-desktop>"
            VERBATIM)
    endif()
endif()
target_link_libraries(bimeron-desktop PRIVATE Qt6::Widgets Qt6::Network core-update)
if(WIN32)
    target_link_libraries(bimeron-desktop PRIVATE shell32 advapi32 ole32)
endif()
target_compile_definitions(bimeron-desktop PRIVATE
    BIMERON_GUI_VERSION="${PROJECT_VERSION}"
    BIMERON_CORE_UPDATE_PUBLIC_KEY="${BIMERON_CORE_UPDATE_PUBLIC_KEY}")
option(BIMERON_LOCAL_UPDATE_TESTING "Use an isolated local Core release feed (test builds only)" OFF)
if(BIMERON_LOCAL_UPDATE_TESTING)
    target_compile_definitions(bimeron-desktop PRIVATE BIMERON_LOCAL_UPDATE_TESTING)
    message(WARNING "Local Core update feed enabled; this build is for isolated testing only")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(developer-mode-test tests/DeveloperModeTest.cpp ${BIMERON_DESKTOP_SOURCES})
    target_include_directories(developer-mode-test PRIVATE src)
    target_link_libraries(developer-mode-test PRIVATE Qt6::Widgets Qt6::Network core-update)
    target_compile_definitions(developer-mode-test PRIVATE
        NATIVE_IKEV2_TRANSPORT_TESTING
        BIMERON_GUI_VERSION="${PROJECT_VERSION}"
        BIMERON_CORE_UPDATE_PUBLIC_KEY="")
    if(WIN32)
        target_link_libraries(developer-mode-test PRIVATE advapi32 shell32 ole32)
    endif()
    add_test(NAME developer-mode-test COMMAND developer-mode-test --visual-check)
    if(NOT WIN32)
        set_tests_properties(developer-mode-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
    add_executable(core-update-test tests/CoreUpdateTest.cpp src/CoreUpdater.cpp src/CoreUpdater.h
        src/TrustedCore.cpp src/TrustedCore.h)
    target_include_directories(core-update-test PRIVATE src third_party/monocypher third_party/miniz)
    target_link_libraries(core-update-test PRIVATE core-update)
    if(WIN32)
        target_link_libraries(core-update-test PRIVATE advapi32 shell32)
    endif()
    target_compile_definitions(core-update-test PRIVATE BIMERON_CORE_UPDATE_PUBLIC_KEY="${BIMERON_CORE_UPDATE_PUBLIC_KEY}")
    add_test(NAME core-update-test COMMAND core-update-test)
    if(BIMERON_LOCAL_UPDATE_TESTING)
        set(BIMERON_LOCAL_UPDATE_TEST_PUBLIC_KEY "" CACHE STRING "Ephemeral local-feed test public key")
        if(BIMERON_LOCAL_UPDATE_TEST_PUBLIC_KEY STREQUAL "")
            message(FATAL_ERROR "Set an ephemeral BIMERON_LOCAL_UPDATE_TEST_PUBLIC_KEY for local-feed fixture tests")
        endif()
        add_executable(core-updater-feed-test tests/CoreUpdaterFeedTest.cpp src/CoreUpdater.cpp src/CoreUpdater.h)
        target_include_directories(core-updater-feed-test PRIVATE src third_party/monocypher)
        target_link_libraries(core-updater-feed-test PRIVATE core-update)
        target_compile_definitions(core-updater-feed-test PRIVATE
            BIMERON_LOCAL_UPDATE_TESTING
            BIMERON_CORE_UPDATE_PUBLIC_KEY="${BIMERON_LOCAL_UPDATE_TEST_PUBLIC_KEY}")
    endif()
    add_executable(profile-vless-test tests/ProfileVlessTest.cpp
        src/ProfileStore.cpp src/ProfileStore.h
        src/ProfileEditorDialog.cpp src/ProfileEditorDialog.h)
    target_include_directories(profile-vless-test PRIVATE src)
    target_link_libraries(profile-vless-test PRIVATE Qt6::Widgets Qt6::Network)
    if(WIN32)
        target_link_libraries(profile-vless-test PRIVATE advapi32)
    endif()
    add_test(NAME profile-vless-test COMMAND profile-vless-test)
    if(NOT WIN32)
        set_tests_properties(profile-vless-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

include(cmake/core-release.cmake)
set(BIMERON_CORE_ROOT "${BIMERON_CORE_ROOT_DEFAULT}" CACHE PATH "Downloaded Bimeron Core payload")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" BIMERON_CORE_PLATFORM)
set(BIMERON_CORE_DIR "${BIMERON_CORE_ROOT}/${BIMERON_CORE_PLATFORM}-${BIMERON_PACKAGE_ARCH}")
set(BIMERON_CORE_EXECUTABLE "${BIMERON_CORE_DIR}/${BIMERON_CORE_EXECUTABLE_${BIMERON_CORE_PLATFORM}}")
set(BIMERON_CORE_LICENSE "${BIMERON_CORE_DIR}/${BIMERON_CORE_LICENSE_FILE}")
set(BIMERON_THIRD_PARTY_NOTICES "${BIMERON_CORE_DIR}/${BIMERON_CORE_NOTICES_FILE}")
if(WIN32)
    set(BIMERON_WINTUN_DLL "${BIMERON_CORE_DIR}/${BIMERON_WINTUN_DLL_FILE}")
    set(BIMERON_WINTUN_LICENSE "${BIMERON_CORE_DIR}/${BIMERON_WINTUN_LICENSE_FILE}")
endif()
option(BIMERON_REQUIRE_CORE "Require complete core payload for packaging" OFF)
if(BIMERON_REQUIRE_CORE)
    if(BIMERON_CORE_UPDATE_PUBLIC_KEY STREQUAL "")
        message(FATAL_ERROR "Package build requires the pinned core update public key; clear the empty BIMERON_CORE_UPDATE_PUBLIC_KEY cache override")
    endif()
    if(BIMERON_CORE_ROOT STREQUAL BIMERON_CORE_ROOT_DEFAULT AND NOT BIMERON_CORE_RELEASE_SUPPORTS_SOURCE_UPDATE)
        message(FATAL_ERROR "Pinned core ${BIMERON_CORE_VERSION} lacks the trusted -source updater; pin an official release with final SHA-256 values before packaging")
    endif()
    foreach(required_file BIMERON_CORE_EXECUTABLE BIMERON_CORE_LICENSE BIMERON_THIRD_PARTY_NOTICES)
        if(NOT EXISTS "${${required_file}}")
            message(FATAL_ERROR "Missing ${required_file}: ${${required_file}}")
        endif()
    endforeach()
    if(WIN32)
        foreach(required_file BIMERON_WINTUN_DLL BIMERON_WINTUN_LICENSE)
            if(NOT EXISTS "${${required_file}}")
                message(FATAL_ERROR "Missing ${required_file}: ${${required_file}}")
            endif()
        endforeach()
    endif()
endif()
if(EXISTS "${BIMERON_CORE_EXECUTABLE}")
    if(WIN32)
        set(BIMERON_CORE_OUTPUT_NAME "bimeron-core.exe")
    else()
        set(BIMERON_CORE_OUTPUT_NAME "bimeron-core")
    endif()
    add_custom_target(bundle-bimeron-core ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BIMERON_CORE_EXECUTABLE}"
            "$<TARGET_FILE_DIR:bimeron-desktop>/${BIMERON_CORE_OUTPUT_NAME}"
        DEPENDS bimeron-desktop "${BIMERON_CORE_EXECUTABLE}"
        VERBATIM)
    if(WIN32 AND EXISTS "${BIMERON_WINTUN_DLL}")
        add_custom_command(TARGET bundle-bimeron-core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BIMERON_WINTUN_DLL}" "$<TARGET_FILE_DIR:bimeron-desktop>/wintun.dll"
            VERBATIM)
    endif()
    if(WIN32 AND EXISTS "${BIMERON_CORE_LICENSE}")
        add_custom_command(TARGET bundle-bimeron-core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BIMERON_CORE_LICENSE}" "$<TARGET_FILE_DIR:bimeron-desktop>/CORE-LICENSE.txt"
            VERBATIM)
    endif()
    if(WIN32 AND EXISTS "${BIMERON_WINTUN_LICENSE}")
        add_custom_command(TARGET bundle-bimeron-core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BIMERON_WINTUN_LICENSE}"
                "$<TARGET_FILE_DIR:bimeron-desktop>/${BIMERON_WINTUN_LICENSE_FILE}"
            VERBATIM)
    endif()
    if(WIN32 AND EXISTS "${BIMERON_THIRD_PARTY_NOTICES}")
        add_custom_command(TARGET bundle-bimeron-core POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BIMERON_THIRD_PARTY_NOTICES}"
                "$<TARGET_FILE_DIR:bimeron-desktop>/${BIMERON_CORE_NOTICES_FILE}"
            VERBATIM)
    endif()
endif()

if(WIN32)
    install(DIRECTORY "$<TARGET_FILE_DIR:bimeron-desktop>/"
        DESTINATION .
        USE_SOURCE_PERMISSIONS
        PATTERN "core-update*" EXCLUDE
        PATTERN "profile-vless-test*" EXCLUDE
        PATTERN "developer-mode-test*" EXCLUDE)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    include(InstallRequiredSystemLibraries)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS MATCHES "vcruntime140_1\\.dll")
        message(FATAL_ERROR "MSVC redistributable runtime was not found for ${BIMERON_PACKAGE_ARCH}")
    endif()
else()
    install(TARGETS bimeron-desktop
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES third_party/monocypher/LICENSE
        DESTINATION share/doc/bimeron-desktop RENAME MONOCYPHER-LICENSE.txt)
    install(FILES third_party/miniz/LICENSE
        DESTINATION share/doc/bimeron-desktop RENAME MINIZ-LICENSE.txt)
    if(EXISTS "${BIMERON_CORE_EXECUTABLE}")
        install(PROGRAMS "${BIMERON_CORE_EXECUTABLE}"
            DESTINATION bin
            RENAME bimeron-core
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    endif()
    install(FILES packaging/linux/bimeron.desktop
        DESTINATION share/applications
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    install(FILES assets/icons/tray/png/light-disconnected-64.png
        DESTINATION share/icons/hicolor/64x64/apps
        RENAME bimeron.png
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    if(EXISTS "${BIMERON_THIRD_PARTY_NOTICES}")
        install(FILES "${BIMERON_THIRD_PARTY_NOTICES}"
            DESTINATION share/doc/bimeron-desktop
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
    if(EXISTS "${BIMERON_CORE_LICENSE}")
        install(FILES "${BIMERON_CORE_LICENSE}"
            DESTINATION share/doc/bimeron-desktop
            RENAME CORE-LICENSE.txt
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
endif()

if(WIN32)
    set(CPACK_PACKAGE_NAME "Bimeron")
else()
    set(CPACK_PACKAGE_NAME "bimeron-desktop")
endif()
set(CPACK_PACKAGE_VENDOR "Bimeron Network")
set(CPACK_PACKAGE_CONTACT "Bimeron Network")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bimeron desktop VPN client")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" BIMERON_PACKAGE_PLATFORM)
set(CPACK_PACKAGE_FILE_NAME
    "bimeron-desktop-${PROJECT_VERSION}-${BIMERON_PACKAGE_PLATFORM}-${BIMERON_PACKAGE_ARCH}")
if(BIMERON_LOCAL_UPDATE_TESTING)
    string(APPEND CPACK_PACKAGE_FILE_NAME "-local-update-test")
endif()
if(WIN32)
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
else()
    # WSL mounts report every file as 0777. Stage DEB payloads on the Linux
    # filesystem so CPack can preserve the requested Unix permissions.
    set(CPACK_PACKAGE_DIRECTORY "/tmp/bimeron-desktop-packages")
endif()
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Bimeron")
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if(EXISTS "${BIMERON_THIRD_PARTY_NOTICES}")
    set(CPACK_RESOURCE_FILE_LICENSE "${BIMERON_THIRD_PARTY_NOTICES}")
endif()

if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "E93DE42D-1F4F-4227-8056-1086581CCE51")
    if(BIMERON_PACKAGE_ARCH STREQUAL "amd64")
        set(CPACK_WIX_ARCHITECTURE "x64")
    else()
        set(CPACK_WIX_ARCHITECTURE "${BIMERON_PACKAGE_ARCH}")
    endif()
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icons/bimeron.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Bimeron")
    set(CPACK_WIX_UI_REF "BimeronUI")
    set(CPACK_WIX_EXTENSIONS "WixUtilExtension")
    set(CPACK_WIX_EXTRA_SOURCES
        "${CMAKE_SOURCE_DIR}/packaging/windows/options.wxs")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "bimeron-desktop")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${BIMERON_PACKAGE_ARCH}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6 (>= 6.4), libqt6network6 (>= 6.4), qt6-translations-l10n, iproute2, nftables, pkexec, systemd")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
endif()

include(CPack)

cmake_minimum_required(VERSION 3.21)
project(ForegroundPrototype LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "Windows-only offline prototype")
endif()
enable_testing()
# Static runtime avoids app-local CRT lookup for the Binary DLL.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_library(foreground-prototype SHARED foreground.cpp)
target_compile_features(foreground-prototype PUBLIC cxx_std_17)
target_compile_definitions(foreground-prototype PRIVATE FOREGROUND_EXPORTS)
target_link_libraries(foreground-prototype PRIVATE advapi32 msi)
target_link_options(foreground-prototype PRIVATE /DEPENDENTLOADFLAG:0x800)
add_executable(foreground-test foreground-test.cpp)
target_link_libraries(foreground-test PRIVATE foreground-prototype advapi32)
add_test(NAME foreground-worker COMMAND foreground-test)
set_tests_properties(foreground-worker PROPERTIES TIMEOUT 30)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  set(WIX_ARCH arm64)
else()
  set(WIX_ARCH x64)
endif()
set(FOREGROUND_DLL "$<TARGET_FILE:foreground-prototype>")
configure_file(foreground-sequence.wxs.in foreground-sequence.configured.wxs @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/foreground-sequence.wxs"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/foreground-sequence.configured.wxs")
add_test(NAME foreground-sequence COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/foreground-sequence-test.py"
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/foreground-sequence.wxs" "${WIX_ARCH}")
# No install rules and no connection to production CPack. Never ship this DLL.

# Optional explicitly supplied cached tools only; never download or install.
set(WIX_TOOL_DIR "" CACHE PATH "Existing WiX 3 tool directory for offline fixture")
if(WIX_TOOL_DIR)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.msi"
    COMMAND "${WIX_TOOL_DIR}/candle.exe" -nologo
      -out "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.wixobj"
      "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/foreground-sequence.wxs"
    COMMAND "${WIX_TOOL_DIR}/light.exe" -nologo
      -out "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.msi"
      "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.wixobj"
    DEPENDS foreground-prototype foreground-sequence.wxs.in VERBATIM)
  add_custom_target(foreground-msi-fixture ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.msi")
  add_test(NAME foreground-msi-readonly COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/foreground-msi-test.py"
    "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/fixture.msi" "$<TARGET_FILE:foreground-prototype>" "${WIX_ARCH}")
endif()

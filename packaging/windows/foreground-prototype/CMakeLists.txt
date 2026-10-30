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

# Separate observation-only lab. Never part of ALL, install, or production CPack.
option(FOREGROUND_LAB_TRACE "Enable explicit offline LABTRACEONLY artifact target" OFF)
if(FOREGROUND_LAB_TRACE)
  if(NOT WIX_TOOL_DIR)
    message(FATAL_ERROR "Lab requires explicitly supplied cached WiX 3 tools")
  endif()
  set(lab_outputs)
  foreach(variant Success CommitFail PostFinalizeFail)
    if(variant STREQUAL "Success")
      set(point 0)
    elseif(variant STREQUAL "CommitFail")
      set(point 1)
    else()
      set(point 2)
    endif()
    add_library(lab-trace-${variant} SHARED EXCLUDE_FROM_ALL lab-trace.cpp)
    target_compile_features(lab-trace-${variant} PRIVATE cxx_std_17)
    target_compile_options(lab-trace-${variant} PRIVATE /W4 /WX)
    target_compile_definitions(lab-trace-${variant} PRIVATE LAB_FAILPOINT=${point})
    target_link_libraries(lab-trace-${variant} PRIVATE advapi32 msi bcrypt)
    target_link_options(lab-trace-${variant} PRIVATE /DEPENDENTLOADFLAG:0x800)
    foreach(kind PRODUCT UPGRADE)
      if(NOT LAB_${variant}_${kind})
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import uuid; print(str(uuid.uuid4()).upper())"
          OUTPUT_VARIABLE guid OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
        set(LAB_${variant}_${kind} "${guid}" CACHE INTERNAL "Immutable lab identity")
      endif()
      set(LAB_${kind} "${LAB_${variant}_${kind}}")
    endforeach()
    set(LAB_DLL "$<TARGET_FILE:lab-trace-${variant}>")
    configure_file(lab-trace.wxs.in lab-trace-${variant}.configured.wxs @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.wxs"
      INPUT "${CMAKE_CURRENT_BINARY_DIR}/lab-trace-${variant}.configured.wxs")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.msi")
    add_custom_command(OUTPUT "${output}"
      COMMAND "${WIX_TOOL_DIR}/candle.exe" -nologo -out "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.wixobj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.wxs"
      COMMAND "${WIX_TOOL_DIR}/light.exe" -nologo -v -out "${output}"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.wixobj"
      DEPENDS lab-trace-${variant} "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/lab-trace-${variant}.wxs" VERBATIM)
    list(APPEND lab_outputs "${output}")
  endforeach()
  add_custom_target(foreground-lab-trace DEPENDS ${lab_outputs})
  add_test(NAME foreground-lab-static COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/lab-trace-check.py" "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>" "${WIX_ARCH}")
endif()

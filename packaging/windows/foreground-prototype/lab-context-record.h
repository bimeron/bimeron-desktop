#pragma once
#include "callback-context.h"
#include <cstddef>
namespace foreground {
constexpr size_t ContextRecordCapacity = 2048;
using ContextObserver = CallbackInteractiveContextObservation (*)();
// Output has a fixed schema and numeric values/hex LUIDs only. No SID text escapes.
bool formatContextRecord(wchar_t* output, size_t capacity, unsigned callback,
    DWORD processSession, DWORD processStatus,
    const CallbackInteractiveContextObservation& observation) noexcept;
bool captureContextRecord(wchar_t* output, size_t capacity, unsigned callback,
    DWORD processSession, DWORD processStatus,
    ContextObserver observer = observeSelectedContext) noexcept;
}

#pragma once
#include <windows.h>
#include <string>
#include <memory>
#ifdef FOREGROUND_EXPORTS
#define FG_API __declspec(dllexport)
#else
#define FG_API __declspec(dllimport)
#endif

namespace foreground {
FG_API bool safeMutexDescriptor(PSECURITY_DESCRIPTOR sd);
FG_API bool markerAbsent(const wchar_t* marker);
// Contract admission: borrowed mutex handle; caller must retain it on this thread.
// On success caller owns mutex; on every failure caller does not own mutex.
FG_API bool admit(HANDLE mutex, const wchar_t* marker);

// Mechanics experiment only: accepts an already-admitted fixture mutex, NOT an
// installer authorization API. No policy/path acceptance or MSI routing exists.
class FG_API Worker {
public:
    Worker(HANDLE mutex, std::wstring journal, std::string rollbackMetadata);
    ~Worker();
    bool begin();
    bool settle(bool rollback, bool completed);
    DWORD ownerThread() const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}

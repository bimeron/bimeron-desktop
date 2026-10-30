#include "foreground.h"
#include <aclapi.h>
#include <msi.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

namespace foreground {
static bool trusted(PSID sid) {
    return sid && (IsWellKnownSid(sid, WinLocalSystemSid) ||
                   IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
}
bool safeMutexDescriptor(PSECURITY_DESCRIPTOR sd) {
    PSID owner = nullptr;
    PACL acl = nullptr;
    BOOL present = FALSE, unused = FALSE;
    if (!sd || !IsValidSecurityDescriptor(sd) ||
        !GetSecurityDescriptorOwner(sd, &owner, &unused) || !trusted(owner) ||
        !GetSecurityDescriptorDacl(sd, &present, &acl, &unused) || !present || !acl ||
        !IsValidAcl(acl)) return false;
    for (DWORD i = 0; i < acl->AceCount; ++i) {
        void* raw = nullptr;
        if (!GetAce(acl, i, &raw)) return false;
        auto header = static_cast<ACE_HEADER*>(raw);
        if (header->AceFlags & INHERIT_ONLY_ACE) continue;
        if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) return false;
        auto ace = static_cast<ACCESS_ALLOWED_ACE*>(raw);
        if (!trusted(&ace->SidStart) && (ace->Mask & ~(READ_CONTROL | SYNCHRONIZE)))
            return false;
    }
    return true;
}
bool markerAbsent(const wchar_t* marker) {
    HANDLE owner = OpenMutexW(SYNCHRONIZE, FALSE, marker);
    DWORD error = GetLastError();
    if (owner) CloseHandle(owner);
    return !owner && error == ERROR_FILE_NOT_FOUND;
}
bool admit(HANDLE mutex, const wchar_t* marker) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    DWORD error = GetSecurityInfo(mutex, SE_KERNEL_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        nullptr, nullptr, nullptr, nullptr, &sd);
    bool safe = error == ERROR_SUCCESS && safeMutexDescriptor(sd);
    if (sd) LocalFree(sd);
    if (!safe) return false;
    DWORD wait = WaitForSingleObject(mutex, 0);
    if (wait == WAIT_ABANDONED) { ReleaseMutex(mutex); return false; }
    if (wait != WAIT_OBJECT_0) return false;
    if (markerAbsent(marker)) return true;
    ReleaseMutex(mutex);
    return false;
}

struct Worker::State {
    HANDLE mutex;
    std::wstring journal;
    std::string metadata;
    std::thread thread;
    std::mutex lock;
    std::mutex callers;
    std::condition_variable cv;
    enum Command { idle, begin, commit, rollback, incomplete, quit } command = idle;
    bool done = false, result = false, held = false, started = false;
    DWORD owner = 0;
    HMODULE module = nullptr;

    bool prepare() {
        // Do not retry abandonment; it grants ownership and must be released.
        DWORD wait = WaitForSingleObject(mutex, 0);
        if (wait == WAIT_ABANDONED) { ReleaseMutex(mutex); return false; }
        if (wait != WAIT_OBJECT_0) return false;
        held = true;
        HANDLE file = CreateFileW(journal.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        std::string record = "foreground-prototype-pending-v1\n" + metadata;
        bool ok = WriteFile(file, record.data(), static_cast<DWORD>(record.size()),
                            &written, nullptr) && written == record.size() &&
                  FlushFileBuffers(file);
        CloseHandle(file);
        // Any partial record remains and refuses the next begin.
        return ok;
    }
    void run() {
        std::unique_lock<std::mutex> guard(lock);
        owner = GetCurrentThreadId();
        for (;;) {
            cv.wait(guard, [&] { return command != idle; });
            Command next = command;
            result = false;
            if (next == begin && !started) {
                started = true;
                result = prepare();
                if (!result && held) { ReleaseMutex(mutex); held = false; }
            } else if ((next == commit || next == rollback) && held) {
                // Surrogate for completed external mutation/rollback. No real
                // file/policy mutation is implemented. Journal survives failure.
                result = DeleteFileW(journal.c_str()) != FALSE;
                if (result) { ReleaseMutex(mutex); held = false; }
            }
            if (next == quit && held) {
                // Host teardown is NOT successful settlement. Preserve journal.
                ReleaseMutex(mutex);
                held = false;
            }
            command = idle;
            done = true;
            cv.notify_all();
            if (next == quit) break;
        }
        // Process-lifetime module pin protects even the worker/destructor tail.
        // It does not keep an MSI server process alive.
    }
    bool send(Command next) {
        std::lock_guard<std::mutex> serialize(callers);
        std::unique_lock<std::mutex> guard(lock);
        cv.wait(guard, [&] { return command == idle; });
        done = false;
        command = next;
        cv.notify_all();
        cv.wait(guard, [&] { return done; });
        return result;
    }
};
Worker::Worker(HANDLE mutex, std::wstring journal, std::string metadata)
    : state(std::make_unique<State>()) {
    state->mutex = mutex;
    state->journal = std::move(journal);
    state->metadata = std::move(metadata);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&safeMutexDescriptor), &state->module))
        throw std::runtime_error("cannot pin prototype DLL");
    state->thread = std::thread([this] { state->run(); });
}
Worker::~Worker() {
    state->send(State::quit);
    state->thread.join();
}
bool Worker::begin() { return state->send(State::begin); }
bool Worker::settle(bool rollback, bool completed) {
    return state->send(!completed ? State::incomplete : rollback ? State::rollback : State::commit);
}
DWORD Worker::ownerThread() const {
    std::lock_guard<std::mutex> guard(state->lock);
    return state->owner;
}
}

// Deliberate hard gate: no accepted final Core artifacts, secure cross-process
// protocol, or proven MSI transaction lifetime. Never authorize a real install.
extern "C" __declspec(dllexport) UINT __stdcall ForegroundPrototype(MSIHANDLE) {
    return ERROR_INSTALL_FAILURE;
}

#include "foreground.h"
#include <sddl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static int probe(const wchar_t* name) {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name);
    if (!mutex) return 2;
    DWORD wait = WaitForSingleObject(mutex, 0);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) ReleaseMutex(mutex);
    CloseHandle(mutex);
    return wait == WAIT_TIMEOUT ? 0 : 3;
}
static void runChild(const std::wstring& arguments) {
    wchar_t image[32768];
    check(GetModuleFileNameW(nullptr, image, 32768) != 0, "image path");
    std::wstring cmd = L"\"" + std::wstring(image) + L"\" " + arguments;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    check(CreateProcessW(image, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                         nullptr, &startup, &process), "child launch");
    check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "child deadline");
    DWORD code = 99;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    check(code == 0, "separate callback process must remain excluded");
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring(argv[1]) == L"--probe") return probe(argv[2]);
    if (argc == 4 && std::wstring(argv[1]) == L"--crash") {
        HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, argv[2]);
        if (!mutex) return 2;
        foreground::Worker worker(mutex, argv[3], "crash-rollback-metadata\n");
        if (!worker.begin()) return 3;
        ExitProcess(0); // Deliberate death of this isolated fixture, no destructors.
    }
    try {
        for (const auto& fixture : {
            std::pair{L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x00100000;;;BU)", true},
            std::pair{L"O:SYG:SYD:P(A;;GA;;;SY)", true},
            std::pair{L"O:BUG:BAD:P(A;;GA;;;BA)", false},
            std::pair{L"O:BAG:BAD:P(A;;GA;;;WD)", false},
            std::pair{L"O:BAG:BAD:P(A;;0x00000001;;;BU)", false},
            std::pair{L"O:BAG:BAD:P(A;;RC;;;BU)", true},
            std::pair{L"O:BAG:BAD:NO_ACCESS_CONTROL", false}}) {
            PSECURITY_DESCRIPTOR sd = nullptr;
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(fixture.first,
                  SDDL_REVISION_1, &sd, nullptr)) {
                std::wcerr << fixture.first << L" error=" << GetLastError() << L'\n';
                throw std::runtime_error("SDDL fixture");
            }
            bool actual = foreground::safeMutexDescriptor(sd);
            LocalFree(sd);
            check(actual == fixture.second, "descriptor decision");
        }
        PSECURITY_DESCRIPTOR unknown = nullptr;
        check(ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"O:BAG:BAD:P(A;;GA;;;BA)", SDDL_REVISION_1, &unknown, nullptr), "unknown ACE fixture");
        PACL acl = nullptr;
        BOOL present, defaulted;
        check(GetSecurityDescriptorDacl(unknown, &present, &acl, &defaulted), "fixture DACL");
        void* ace = nullptr;
        check(GetAce(acl, 0, &ace), "fixture ACE");
        static_cast<ACE_HEADER*>(ace)->AceType = SYSTEM_AUDIT_ACE_TYPE;
        check(!foreground::safeMutexDescriptor(unknown), "unknown effective ACE accepted");
        LocalFree(unknown);
        std::wstring name = L"Local\\Bimeron.Foreground.Prototype." +
                            std::to_wstring(GetCurrentProcessId()) + L"." +
                            std::to_wstring(GetTickCount64());
        HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
        check(mutex != nullptr, "fixture mutex");
        std::wstring marker = name + L".Marker";
        check(foreground::markerAbsent(marker.c_str()), "absent marker refused");
        HANDLE owner = CreateMutexW(nullptr, FALSE, marker.c_str());
        check(owner && !foreground::markerAbsent(marker.c_str()), "present marker ignored");
        CloseHandle(owner);
        HANDLE wrongType = CreateEventW(nullptr, TRUE, FALSE, marker.c_str());
        check(wrongType && !foreground::markerAbsent(marker.c_str()), "wrong-type marker ignored");
        CloseHandle(wrongType);
        PSECURITY_DESCRIPTOR denied = nullptr;
        check(ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(D;;0x00100000;;;WD)", SDDL_REVISION_1, &denied, nullptr), "denied marker SDDL");
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), denied, FALSE};
        HANDLE inaccessible = CreateMutexW(&attributes, FALSE, marker.c_str());
        check(inaccessible && !foreground::markerAbsent(marker.c_str()), "denied marker ignored");
        CloseHandle(inaccessible);
        LocalFree(denied);
        // Actual GetSecurityInfo admission rejects a user-owned fixture even
        // though CreateMutex succeeded. No elevation or Global objects needed.
        check(!foreground::admit(mutex, (name + L".Marker").c_str()),
              "untrusted actual object admitted");
        wchar_t temp[MAX_PATH];
        check(GetTempPathW(MAX_PATH, temp) != 0, "temp directory");
        auto directory = std::filesystem::path(temp) /
            (L"bimeron-foreground-prototype-" + std::to_wstring(GetCurrentProcessId()) +
             L"-" + std::to_wstring(GetTickCount64()));
        check(std::filesystem::create_directory(directory), "unique fixture directory");
        auto journal = directory / L"journal";
        const std::string metadata = "installation=0123456789abcdef0123456789abcdef\n"
            "prior-policy={schema:1,opaque-test-bytes}\nrollback=fixture-image-identity\n";
        for (bool rollback : {false, true}) {
            foreground::Worker worker(mutex, journal.wstring(), metadata);
            check(worker.begin(), "worker begin");
            check(!worker.begin(), "duplicate admission accepted");
            DWORD owner = worker.ownerThread();
            check(owner != GetCurrentThreadId(), "dedicated OS thread");
            check(probe(name.c_str()) == 0, "callback thread exclusion");
            runChild(L"--probe " + name);
            // Model a nested uninstall's independent worker: cannot acquire.
            {
                foreground::Worker nested(mutex, (directory / L"nested").wstring(), metadata);
                check(!nested.begin(), "nested uninstall admitted");
            }
            std::ifstream input(journal, std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(input)), {});
            input.close();
            check(bytes == "foreground-prototype-pending-v1\n" + metadata,
                  "rollback metadata changed");
            check(!worker.settle(rollback, false), "incomplete settlement accepted");
            check(probe(name.c_str()) == 0, "released before completed settlement");
            HANDLE shared = CreateFileW(journal.c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            check(shared != INVALID_HANDLE_VALUE, "sharing fixture");
            check(!worker.settle(rollback, true), "sharing violation accepted");
            check(probe(name.c_str()) == 0, "sharing violation released guard");
            CloseHandle(shared);
            bool settled = false;
            std::thread callback([&] { settled = worker.settle(rollback, true); });
            callback.join();
            check(settled && owner == worker.ownerThread(), "settlement thread changed");
            check(!worker.settle(rollback, true), "duplicate settlement accepted");
            check(!std::filesystem::exists(journal), "settled journal retained");
            check(probe(name.c_str()) == 3, "mutex not released after settlement");
        }
        {
            foreground::Worker failed(mutex, (directory / L"missing" / L"journal").wstring(), metadata);
            check(!failed.begin(), "journal failure admitted mutation");
            check(probe(name.c_str()) == 3, "journal failure retained mutex");
        }
        {
            foreground::Worker worker(mutex, journal.wstring(), metadata);
            check(worker.begin(), "teardown begin");
        }
        check(std::filesystem::exists(journal), "host teardown erased pending journal");
        {
            foreground::Worker restart(mutex, journal.wstring(), metadata);
            check(!restart.begin(), "incomplete durable journal accepted");
        }
        std::filesystem::remove(journal); // Explicit fixture cleanup only.
        std::thread abandon([&] {
            check(WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0, "abandon acquire");
        });
        abandon.join();
        {
            foreground::Worker worker(mutex, journal.wstring(), metadata);
            check(!worker.begin(), "abandoned mutex admitted");
        }
        check(probe(name.c_str()) == 3, "abandoned ownership not released");
        check(!std::filesystem::exists(journal), "mutation after abandonment");
        runChild(L"--crash " + name + L" \"" + journal.wstring() + L"\"");
        check(std::filesystem::exists(journal), "process crash lost flushed journal");
        {
            foreground::Worker restart(mutex, journal.wstring(), metadata);
            check(!restart.begin(), "process crash admitted");
        }
        // Abandonment was consumed; durable record must still block admission.
        {
            foreground::Worker restart(mutex, journal.wstring(), metadata);
            check(!restart.begin(), "post-abandonment journal admitted");
        }
        std::filesystem::remove(journal);
        CloseHandle(mutex);
        std::filesystem::remove(directory);
        HMODULE dll = GetModuleHandleW(L"foreground-prototype.dll");
        using Action = UINT (__stdcall*)(UINT_PTR);
        auto action = reinterpret_cast<Action>(GetProcAddress(dll, "ForegroundPrototype"));
        check(action && action(0) == ERROR_INSTALL_FAILURE, "production gate open");
        std::cout << "PASS: security, worker thread/process exclusion, nested refusal, "
                     "journal/settlement/teardown, abandonment, hard MSI gate\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

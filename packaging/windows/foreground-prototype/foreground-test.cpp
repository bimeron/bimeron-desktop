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
static void contextChecks() {
    using namespace foreground;
    CallbackInteractiveContextObservation model;
    model.source = TokenSource::Process; model.type = TokenPrimary;
    model.elevationType = TokenElevationTypeDefault; // nonsplit administrator
    model.queried = model.elevated = model.enabledAdmin = model.lsaDataValid = true;
    model.restricted = model.appContainer = false;
    model.integrity = SECURITY_MANDATORY_HIGH_RID;
    model.session = model.lsaSession = 1;
    model.sid = model.lsaSid = L"S-1-5-21-1-2-3-1001";
    model.authenticationId.LowPart = model.lsaAuthenticationId.LowPart = 7;
    model.logonType = 2;
    check(callbackContextRefusalFixture(model) == ContextRefusal::None, "MODEL nonsplit positive");
    auto remote = model; remote.logonType = 10;
    check(callbackContextRefusalFixture(remote) == ContextRefusal::None, "MODEL remote interactive");
    for (int n = 0; n < 28; ++n) {
        auto bad = model;
        switch (n) {
        case 0: bad.queried = false; break;
        case 1: bad.source = TokenSource::Unknown; break;
        case 2: bad.elevated = false; bad.elevationType = TokenElevationTypeFull; break;
        case 3: bad.enabledAdmin = false; break;
        case 4: bad.adminDenyOnly = true; break;
        case 5: bad.restricted = true; break;
        case 6: bad.appContainer = true; break;
        case 7: bad.integrity = SECURITY_MANDATORY_MEDIUM_RID; break;
        case 8: bad.type = TokenImpersonation; bad.impersonation = SecurityIdentification; break;
        case 9: bad.session = 0; break;
        case 10: bad.sid = bad.lsaSid = L"S-1-5-18"; break;
        case 11: bad.sid = bad.lsaSid = L"S-1-5-19"; break;
        case 12: bad.sid = bad.lsaSid = L"S-1-5-20"; break;
        case 13: bad.sid = bad.lsaSid = L"S-1-5-80-123"; break;
        case 14: bad.sid.clear(); break;
        case 15: bad.authenticationId = {}; break;
        case 16: bad.lsaStatus = static_cast<LONG>(0xc0000022); break;
        case 17: bad.lsaDataValid = false; break;
        case 18: bad.lsaSid += L"0"; break;
        case 19: ++bad.lsaSession; break;
        case 20: ++bad.lsaAuthenticationId.HighPart; break;
        case 21: bad.type = static_cast<TOKEN_TYPE>(0); break;
        case 22: bad.type = TokenImpersonation; bad.impersonation = SecurityAnonymous; break;
        case 23: bad.servicePrincipal = true; break;
        case 24: bad.status = ERROR_ACCESS_DENIED; break;
        case 25: bad.elevationType = TokenElevationTypeLimited; break;
        case 26: bad.elevationType = static_cast<TOKEN_ELEVATION_TYPE>(0); break;
        case 27: bad.type = TokenImpersonation; bad.impersonation = static_cast<SECURITY_IMPERSONATION_LEVEL>(4); break;
        }
        check(callbackContextRefusalFixture(bad) != ContextRefusal::None, "MODEL unsafe context accepted");
    }
    for (DWORD type = 0; type < 16; ++type) {
        auto bad = model; bad.logonType = type;
        check((callbackContextRefusalFixture(bad) == ContextRefusal::None) == (type == 2 || type == 10),
              "MODEL exact logon type allowlist");
    }
    SECURITY_LOGON_SESSION_DATA lsa{};
    PSID sid = nullptr;
    check(ConvertStringSidToSidW(model.sid.c_str(), &sid), "MODEL logon SID");
    lsa.Size = sizeof(lsa); lsa.Sid = sid; lsa.LogonId = model.authenticationId;
    lsa.Session = 1; lsa.LogonType = 2;
    auto parsed = model;
    captureLogonDataFixture(parsed, &lsa);
    check(callbackContextRefusalFixture(parsed) == ContextRefusal::None, "MODEL logon parser positive");
    for (int n = 0; n < 5; ++n) {
        auto malformed = lsa;
        BYTE invalidSid[SECURITY_MAX_SID_SIZE]{};
        switch (n) {
        case 0: malformed.Size = 0; break;
        case 1: malformed.Size = 65537; break;
        case 2: malformed.Sid = nullptr; break;
        case 3: malformed.Sid = invalidSid; break;
        case 4: break;
        }
        captureLogonDataFixture(parsed, n == 4 ? nullptr : &malformed);
        check(!parsed.lsaDataValid && callbackContextRefusalFixture(parsed) != ContextRefusal::None,
              "MODEL malformed LSA accepted");
    }
    LocalFree(sid);
    auto current = observeCallbackInteractiveContext();

    std::cout << "NATIVE current context refusal=" << static_cast<int>(current.refusal)
              << " status=" << current.status << " lsaStatus=" << current.lsaStatus << '\n';
    check(current.source == TokenSource::Process && current.queried, "NATIVE current process token query");
    HANDLE process = nullptr;
    check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &process), "native process token");
    TOKEN_STATISTICS processStats{}; DWORD processSize = 0;
    check(GetTokenInformation(process, TokenStatistics, &processStats, sizeof(processStats), &processSize),
          "native process statistics");
    check(current.selectedTokenId.LowPart == processStats.TokenId.LowPart &&
          current.selectedTokenId.HighPart == processStats.TokenId.HighPart, "NATIVE process fallback TokenId");
    TokenFixture legacy;
    check(readEffectiveToken(legacy), "NATIVE legacy current token reader");
    for (auto level : {SecurityImpersonation, SecurityIdentification, SecurityAnonymous}) {
        HANDLE token = nullptr;
        if (!DuplicateTokenEx(process, TOKEN_QUERY | TOKEN_IMPERSONATE, nullptr, level, TokenImpersonation, &token)) {
            std::cout << "UNAVAILABLE native duplicate level=" << level << " status=" << GetLastError() << '\n';
            continue;
        }
        TOKEN_STATISTICS stats{}; DWORD size = 0;
        check(GetTokenInformation(token, TokenStatistics, &stats, sizeof(stats), &size), "native selected token ID");
        if (SetThreadToken(nullptr, token)) {
            auto selected = observeCallbackInteractiveContext();
            // Restore only in test teardown, never to acquire callback authority.
            check(SetThreadToken(nullptr, nullptr), "restore native test thread");
            if (selected.queried) {
                check(selected.source == TokenSource::Thread &&
                      selected.selectedTokenId.LowPart == stats.TokenId.LowPart &&
                      selected.selectedTokenId.HighPart == stats.TokenId.HighPart, "NATIVE exact selected TokenId");
            } else {
                check(selected.refusal != ContextRefusal::None && selected.source != TokenSource::Process,
                      "NATIVE failed thread query used process");
                std::cout << "UNAVAILABLE native query level=" << level << " status=" << selected.status << '\n';
            }
            if (level < SecurityImpersonation)
                check(selected.refusal != ContextRefusal::None, "NATIVE weak impersonation accepted");
        } else std::cout << "UNAVAILABLE native impersonation level=" << level << " status=" << GetLastError() << '\n';
        CloseHandle(token);
        std::cout << "NATIVE selected-token/weak-level checks completed level=" << level << '\n';
    }
    HANDLE restricted = nullptr;
    check(CreateRestrictedToken(process, DISABLE_MAX_PRIVILEGE, 0, nullptr, 0, nullptr, 0, nullptr, &restricted),
          "native restricted token");
    HANDLE impersonation = nullptr;
    check(DuplicateTokenEx(restricted, TOKEN_QUERY | TOKEN_IMPERSONATE, nullptr, SecurityImpersonation,
                          TokenImpersonation, &impersonation), "native restricted impersonation");
    check(SetThreadToken(nullptr, impersonation), "select restricted token");
    auto limited = observeCallbackInteractiveContext();
    check(SetThreadToken(nullptr, nullptr), "restore restricted test thread");
    check(limited.queried && limited.source == TokenSource::Thread && limited.restricted &&
          limited.refusal != ContextRefusal::None, "NATIVE restricted negative");
    CloseHandle(impersonation); CloseHandle(restricted);
    std::cout << "NATIVE restricted thread token refused\n";
    PSECURITY_DESCRIPTOR deny = nullptr;
    check(ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAG:BAD:P(D;;KA;;;WD)(A;;KA;;;BA)", SDDL_REVISION_1, &deny, nullptr), "deny descriptor");
    auto observedDeny = observeCallbackInteractiveContext(deny);
    check(observedDeny.refusal != ContextRefusal::None, "NATIVE adapter descriptor denial");
    HANDLE explicitToken = nullptr;
    check(DuplicateToken(process, SecurityImpersonation, &explicitToken), "explicit access token");
    check(!journalAccessFixture(deny, explicitToken, true, true), "NATIVE in-memory descriptor access denied");
    CloseHandle(explicitToken); LocalFree(deny); CloseHandle(process);
    std::cout << "MODEL context/full-field/LSA mismatch/type checks passed; protected-object access unavailable\n";
}
static int observeContextOnly() {
    const auto o = foreground::observeCallbackInteractiveContext();
    // LSA failure can be masked by an earlier policy refusal; report it separately.
    const bool complete = o.queried && o.status == 0 && o.lsaStatus == 0 && o.lsaDataValid;
    std::cout << "CONTEXT_ONLY complete=" << complete
              << " eligible=" << (complete && o.refusal == foreground::ContextRefusal::None)
              << " refusal=" << static_cast<int>(o.refusal)
              << " status=" << o.status << " lsaStatus=" << o.lsaStatus
              << " source=" << static_cast<int>(o.source)
              << " type=" << static_cast<int>(o.type)
              << " elevationType=" << static_cast<int>(o.elevationType)
              << " impersonation=" << static_cast<int>(o.impersonation)
              << " queried=" << o.queried << " elevated=" << o.elevated
              << " enabledAdmin=" << o.enabledAdmin << " adminDenyOnly=" << o.adminDenyOnly
              << " restricted=" << o.restricted << " appContainer=" << o.appContainer
              << " servicePrincipal=" << o.servicePrincipal
              << " integrity=" << o.integrity << " session=" << o.session
              << " lsaDataValid=" << o.lsaDataValid << " lsaSession=" << o.lsaSession
              << " logonType=" << o.logonType << '\n';
    return complete ? 0 : 1;
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring(argv[1]) == L"--observe-context-only")
        return observeContextOnly();
    if (argc == 3 && std::wstring(argv[1]) == L"--probe") return probe(argv[2]);
    if (argc == 4 && std::wstring(argv[1]) == L"--crash") {
        HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, argv[2]);
        if (!mutex) return 2;
        foreground::Worker worker(mutex, argv[3], "crash-rollback-metadata\n");
        if (!worker.begin()) return 3;
        ExitProcess(0); // Deliberate death of this isolated fixture, no destructors.
    }
    if (argc != 1) {
        std::cerr << "CONTEXT_CLI invalidArguments=1\n";
        return 2;
    }
    try {
        contextChecks();
        for (const auto& fixture : {
            std::pair{L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x00100000;;;BU)", true},
            std::pair{L"O:SYG:SYD:P(A;;GA;;;SY)", true},
            std::pair{L"O:BUG:BAD:P(A;;GA;;;BA)", false},
            std::pair{L"O:BAG:BAD:P(A;;GA;;;WD)", false},
            std::pair{L"O:BAG:BAD:P(A;;0x00000001;;;BU)", false},
            std::pair{L"O:BAG:BAD:P(A;;RC;;;BU)", true},
            std::pair{L"O:BAG:BAD:NO_ACCESS_CONTROL", false},
            std::pair{L"O:BAG:BA", false}}) {
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
        check(!foreground::safeJournalDescriptor(unknown), "unknown journal ACE accepted");
        check(!foreground::safeJournalDescriptor(nullptr), "unreadable journal descriptor accepted");
        LocalFree(unknown);
        std::wstring name = L"Local\\Bimeron.Foreground.Prototype." +
                            std::to_wstring(GetCurrentProcessId()) + L"." +
                            std::to_wstring(GetTickCount64());
        HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
        check(mutex != nullptr, "fixture mutex");
        std::wstring marker = name + L".Marker";
        check(foreground::markerAbsent(marker.c_str()), "absent marker refused");
        HANDLE markerOwner = CreateMutexW(nullptr, FALSE, marker.c_str());
        check(markerOwner && !foreground::markerAbsent(marker.c_str()), "present marker ignored");
        CloseHandle(markerOwner);
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
        foreground::SettlementFixture evidence;
        evidence.callback = foreground::Callback::PostFinalize;
        evidence.pendingGeneration = evidence.observedGeneration = 42;
        evidence.pending = evidence.inventory = evidence.outerCompletion = true;
        evidence.protection = evidence.quiescent = evidence.flushed = evidence.hostAlive = true;
        auto& t = evidence.effective;
        t.observed = t.interactive = t.elevated = t.enabledAdmin = t.impersonationAdequate = true;
        t.restricted = false; t.integrity = SECURITY_MANDATORY_HIGH_RID;
        t.session = 1; t.sid = L"S-1-5-21-1"; t.logon.LowPart = 7;
        evidence.initiating = t;
        auto filtered = t; filtered.elevated = false;
        using TR = foreground::ThreadTokenResult;
        check(!foreground::eligibleContextFixture(
            foreground::effectiveTokenFixture(TR::Present, filtered, t), t), "process overrode filtered thread");
        check(!foreground::eligibleContextFixture(
            foreground::effectiveTokenFixture(TR::Error, {}, t), t), "thread error fell back to process");
        check(foreground::eligibleContextFixture(
            foreground::effectiveTokenFixture(TR::NoToken, {}, t), t), "no-token process fallback");
        check(foreground::settledFixture(evidence), "bounded fixture should settle");
        check(!foreground::settledFixture({}), "default evidence settled");
        for (auto callback : {foreground::Callback::Missing, foreground::Callback::Commit,
                              foreground::Callback::Rollback}) {
            auto bad = evidence; bad.callback = callback;
            check(!foreground::settledFixture(bad), "callback settled pending");
        }
        for (bool foreground::SettlementFixture::* field : {
                &foreground::SettlementFixture::pending, &foreground::SettlementFixture::inventory,
                &foreground::SettlementFixture::outerCompletion, &foreground::SettlementFixture::protection,
                &foreground::SettlementFixture::quiescent, &foreground::SettlementFixture::flushed,
                &foreground::SettlementFixture::hostAlive}) {
            auto bad = evidence; bad.*field = false;
            check(!foreground::settledFixture(bad), "missing settlement primitive accepted");
        }
        for (int n = 0; n < 10; ++n) {
            auto bad = evidence;
            switch (n) {
            case 0: bad.effective.elevated = false; break;
            case 1: bad.effective.enabledAdmin = false; break;
            case 2: bad.effective.restricted = true; break;
            case 3: bad.effective.integrity = SECURITY_MANDATORY_MEDIUM_RID; break;
            case 4: bad.effective.session = 0; break;
            case 5: bad.effective.sid = L"S-1-5-18"; break;
            case 6: bad.effective.logon.LowPart++; break;
            case 7: bad.effective.impersonationAdequate = false; break;
            case 8: bad.effective.interactive = false; break;
            case 9: bad.observedGeneration++; break;
            }
            check(!foreground::settledFixture(bad), "unsafe context/generation accepted");
        }
        check(foreground::verifyInitiatingClient() == foreground::ClientCorrelation::Unknown,
              "invented initiating-client proof");
        for (auto text : {L"O:BAG:BAD:P(A;;KA;;;BA)",
                          L"O:BAG:BAD:P(A;;KW;;;BU)",
                          L"O:BAG:BAD:P(A;;GW;;;BU)",
                          L"O:BAG:BAD:P(A;;GA;;;WD)",
                          L"O:BAG:BAD:P(A;;0x00000020;;;BU)",
                          L"O:BAG:BAD:P(A;;SD;;;BU)",
                          L"O:BAG:BAD:P(A;;WD;;;BU)",
                          L"O:BAG:BAD:P(A;;WO;;;BU)",
                          L"O:BAG:BAD:P(A;ID;KW;;;BU)",
                          L"O:BAG:BAD:NO_ACCESS_CONTROL",
                          L"O:BAG:BA",
                          L"O:BAG:BAD:(A;;KA;;;BA)",
                          L"O:BUG:BAD:P(A;;KA;;;BA)"}) {
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            check(ConvertStringSecurityDescriptorToSecurityDescriptorW(text, SDDL_REVISION_1,
                  &descriptor, nullptr), "journal descriptor fixture");
            bool safe = foreground::safeJournalDescriptor(descriptor);
            check(safe == (std::wstring(text) == L"O:BAG:BAD:P(A;;KA;;;BA)"), "journal descriptor decision");
            check(!foreground::journalAccessFixture(descriptor, nullptr, true, true), "invalid token access");
            check(!foreground::journalAccessFixture(descriptor, nullptr, false, true), "wrong view access");
            LocalFree(descriptor);
        }
        for (int attempt = 0; attempt < 2; ++attempt) {
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
            check(!worker.settleFixture({}), "incomplete settlement accepted");
            auto commitCallback = evidence;
            commitCallback.callback = foreground::Callback::Commit;
            check(!worker.settleFixture(commitCallback) && std::filesystem::exists(journal),
                  "commit callback erased pending journal");
            check(probe(name.c_str()) == 0, "released before completed settlement");
            HANDLE shared = CreateFileW(journal.c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            check(shared != INVALID_HANDLE_VALUE, "sharing fixture");
            check(!worker.settleFixture(evidence), "sharing violation accepted");
            check(probe(name.c_str()) == 0, "sharing violation released guard");
            CloseHandle(shared);
            bool settled = false;
            std::thread callback([&] { settled = worker.settleFixture(evidence); });
            callback.join();
            check(settled && owner == worker.ownerThread(), "settlement thread changed");
            check(!worker.settleFixture(evidence), "duplicate settlement accepted");
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
        auto post = reinterpret_cast<Action>(GetProcAddress(dll, "ForegroundPostFinalize"));
        check(post && post(0) == ERROR_INSTALL_FAILURE, "post-Finalize production gate open");
        std::cout << "PASS: security, worker thread/process exclusion, nested refusal, "
                     "journal/settlement/teardown, abandonment, hard MSI gate\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

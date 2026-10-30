#include "lab-protected-file.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <string>

static bool same(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
static bool readRecord(const std::wstring& path, labfile::Bytes& bytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info{}; DWORD n = 0;
    bool ok = GetFileInformationByHandle(h, &info) && labfile::safeMetadata(info, false) &&
        info.nFileSizeHigh == 0 && info.nFileSizeLow == bytes.size() &&
        ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &n, nullptr) && n == bytes.size();
    CloseHandle(h); return ok;
}
static bool randomId(labfile::Id& id) {
    return BCryptGenRandom(nullptr, id.data(), static_cast<ULONG>(id.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 &&
        std::any_of(id.begin(), id.end(), [](unsigned char b) { return b != 0; });
}
static void hex(const labfile::Id& id) { for (auto b : id) std::printf("%02x", static_cast<unsigned>(b)); }
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && wcscmp(argv[1], L"--verify-only") == 0) return 0;
    if (argc != 4 || wcscmp(argv[1], L"--guest-pending-file-once") ||
        wcscmp(argv[2], L"--lab-only") || wcscmp(argv[3], L"DisposableWindowsVM")) return 2;
    PWSTR known = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &known))) return 3;
    std::wstring base = std::wstring(known) + L"\\BimeronProtectedFileLab"; CoTaskMemFree(known);
    wchar_t image[32768]; DWORD length = GetModuleFileNameW(nullptr, image, 32768);
    if (!length || length >= 32768 || !same(std::wstring(image, length), base + L"\\lab-protected-file-guest.exe")) return 3;
    if (base.size() < 3 || base[1] != L':' || base[2] != L'\\' ||
        GetDriveTypeW(base.substr(0, 3).c_str()) != DRIVE_FIXED) return 3;
    wchar_t volume[MAX_PATH];
    if (!GetVolumeNameForVolumeMountPointW(base.substr(0, 3).c_str(), volume, MAX_PATH)) return 3;
    const std::wstring pending = std::wstring(volume) + base.substr(3) + L"\\LABProtectedFiles\\pending.bin";
    labfile::Directory directory;
    if (!directory.pin()) return 4;
    DWORD attributes = GetFileAttributesW(pending.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND) return 5;
    labfile::Id installation{}, generation{};
    if (!randomId(installation) || !randomId(generation) || !directory.create(installation, generation)) return 6;
    labfile::Bytes before{}, after{}; labfile::Record record{};
    if (!readRecord(pending, before) || !labfile::decode(before.data(), before.size(), record) ||
        record.installation != installation || record.generation != generation || record.counter != 0) return 7;
    if (!directory.acknowledge(installation, generation) || !readRecord(pending, before) ||
        !labfile::decode(before.data(), before.size(), record) || record.counter != 1 ||
        record.installation != installation || record.generation != generation) return 8;
    auto unchanged = [&]() { return readRecord(pending, after) && before == after; };
    if (directory.create(installation, generation) || !unchanged()) return 9;
    labfile::Id wrong = installation; wrong[0] ^= 1;
    if (std::all_of(wrong.begin(), wrong.end(), [](unsigned char b) { return b == 0; })) wrong[1] = 1;
    if (directory.acknowledge(wrong, generation) || !unchanged()) return 10;
    wrong = generation; wrong[0] ^= 1;
    if (std::all_of(wrong.begin(), wrong.end(), [](unsigned char b) { return b == 0; })) wrong[1] = 1;
    if (directory.acknowledge(installation, wrong) || !unchanged()) return 11;
    HANDLE held = CreateFileW(pending.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (held == INVALID_HANDLE_VALUE) return 12;
    bool refused = !directory.acknowledge(installation, generation); CloseHandle(held);
    if (!refused || !unchanged()) return 12;
    std::printf("{\"state\":\"Pending\",\"counter\":1,\"installation\":\""); hex(installation);
    std::printf("\",\"generation\":\""); hex(generation); std::printf("\"}\n");
    return 0;
}

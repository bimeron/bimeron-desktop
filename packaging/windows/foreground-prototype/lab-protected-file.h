#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <vector>
#include <string>
namespace labfile {
using Id = std::array<unsigned char, 16>;
using Bytes = std::array<unsigned char, 48>;
struct Record { Id installation{}, generation{}; unsigned counter = 0; };
bool encode(const Record&, Bytes&);
bool decode(const unsigned char*, size_t, Record&);
bool advance(Record&, const Id&, const Id&);
bool safeMetadata(const BY_HANDLE_FILE_INFORMATION&, bool directory);
bool sameIdentity(const BY_HANDLE_FILE_INFORMATION&, const BY_HANDLE_FILE_INFORMATION&);
bool safeDescriptor(PSECURITY_DESCRIPTOR);
bool contained(const std::wstring&, const std::wstring&);
bool isAnchor(const std::wstring&, const std::wstring&);
class Directory {
public:
    Directory() = default;
    ~Directory();
    Directory(const Directory&) = delete;
    Directory& operator=(const Directory&) = delete;
    bool pin();
    bool create(const Id&, const Id&);
    bool acknowledge(const Id&, const Id&);
private:
    std::vector<HANDLE> pins;
    std::wstring path;
    size_t authority = 0;
    bool operate(const Id&, const Id&, bool);
};
#ifdef LAB_PROTECTED_FILE_TESTING
enum class Failure { None, ShortWrite, Flush };
bool testWrite(HANDLE, const Record&, Failure);
#endif
}

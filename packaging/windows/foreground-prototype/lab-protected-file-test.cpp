#include "lab-protected-file.h"
#include <sddl.h>
#include <cstdio>
#include <cstdlib>
using namespace labfile;
static void check(bool b, int line) { if(!b) { std::printf("FAILED line %d error %lu\n",line,GetLastError()); std::exit(1); } }
#define require(b) check((b),__LINE__)
static bool acl(const wchar_t* text) {
    PSECURITY_DESCRIPTOR sd=nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(text,SDDL_REVISION_1,&sd,nullptr)!=FALSE);
    bool safe=safeDescriptor(sd); LocalFree(sd); return safe;
}
static void nativeCaseFixture(const std::wstring& parent) {
    const std::wstring dir=parent+L"\\lab-owned-case-"+std::to_wstring(GetCurrentProcessId());
    require(CreateDirectoryW(dir.c_str(),nullptr)!=FALSE);
    HANDLE d=CreateFileW(dir.c_str(),FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    require(d!=INVALID_HANDLE_VALUE);
    FILE_CASE_SENSITIVE_INFO flags{FILE_CS_FLAG_CASE_SENSITIVE_DIR};
    if(!SetFileInformationByHandle(d,FileCaseSensitiveInfo,&flags,sizeof(flags))) {
        const DWORD error=GetLastError();
        CloseHandle(d); require(RemoveDirectoryW(dir.c_str())!=FALSE);
        std::printf("NATIVE case namespace: unavailable, SetFileInformationByHandle error %lu; no case-sensitive opens exercised\n",error);
        return;
    }
    require(!safeDirectoryCase(d));
    FILE_CASE_SENSITIVE_INFO observed{};
    require(GetFileInformationByHandleEx(d,FileCaseSensitiveInfo,&observed,sizeof(observed))!=FALSE);
    require(observed.Flags==FILE_CS_FLAG_CASE_SENSITIVE_DIR);
    const std::wstring lower=dir+L"\\anchor", upper=dir+L"\\ANCHOR";
    HANDLE a=CreateFileW(lower.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,nullptr); require(a!=INVALID_HANDLE_VALUE);
    HANDLE absent=CreateFileW(upper.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,nullptr);
    require(absent==INVALID_HANDLE_VALUE && GetLastError()==ERROR_FILE_NOT_FOUND);
    HANDLE b=CreateFileW(upper.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,nullptr); require(b!=INVALID_HANDLE_VALUE);
    BY_HANDLE_FILE_INFORMATION ai{},bi{};
    require(GetFileInformationByHandle(a,&ai) && GetFileInformationByHandle(b,&bi));
    require(!sameIdentity(ai,bi));
    CloseHandle(a); CloseHandle(b);
    require(DeleteFileW(lower.c_str()) && DeleteFileW(upper.c_str()));
    CloseHandle(d); require(RemoveDirectoryW(dir.c_str())!=FALSE);
    std::puts("NATIVE case namespace: enabled; differently cased OPEN_EXISTING refused, CREATE_NEW opened distinct file IDs; directory rejected");
}
int main() {
    Record r; r.installation[0]=1; r.generation[15]=2; Bytes b; require(encode(r,b));
    Bytes expected{}; expected[0]=0x42; expected[1]=0x4c; expected[2]=0x50; expected[3]=0x46;
    expected[4]=1; expected[5]=1; expected[8]=1; expected[39]=2; require(b==expected);
    Record out; require(decode(b.data(),b.size(),out));
    require(out.installation==r.installation && out.generation==r.generation && out.counter==0);
    for(size_t n=0;n<8;++n) { auto bad=b; bad[n]^=0x80; require(!decode(bad.data(),bad.size(),out)); }
    for(size_t n=44;n<48;++n) { auto bad=b; bad[n]=1; require(!decode(bad.data(),bad.size(),out)); }
    for(size_t n=0;n<48;++n) require(!decode(b.data(),n,out));
    require(!decode(b.data(),49,out)); require(!decode(nullptr,48,out));
    auto bad=b; for(size_t n=8;n<24;++n) bad[n]=0; require(!decode(bad.data(),48,out));
    auto stale=r.generation; stale[0]=9; auto wrong=r.installation; wrong[0]=7;
    require(!advance(r,r.installation,stale) && r.counter==0);
    require(!advance(r,wrong,r.generation) && r.counter==0);
    require(advance(r,r.installation,r.generation) && r.counter==1);
    r.counter=1024; require(encode(r,b) && decode(b.data(),48,out));
    require(!advance(r,r.installation,r.generation) && r.counter==1024);
    auto overflow=b; overflow[40]=0xff; overflow[41]=0xff; overflow[42]=0xff; overflow[43]=0xff;
    require(!decode(overflow.data(),48,out)); require(b[40]==0 && b[41]==4 && b[42]==0 && b[43]==0);
    r.counter=1025; require(!encode(r,b)); r.counter=0;
    require(acl(L"O:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FR;;;BU)"));
    const wchar_t* ti=L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
    std::wstring trusted=L"O:"; trusted+=ti; trusted+=L"D:P(A;;FA;;;"; trusted+=ti; trusted+=L")";
    require(acl(trusted.c_str()));
    require(!acl(L"O:S-1-5-80-1D:P(A;;FA;;;SY)"));
    require(!acl(L"O:BAD:P(A;;FA;;;S-1-5-80-1)"));
    const std::wstring root=L"\\\\?\\Volume{01234567-0123-0123-0123-0123456789ab}\\Program Files";
    require(contained(root,root+L"\\Lab\\test.exe"));
    require(safeCaseFlags(true,0));
    require(!safeDirectoryCase(INVALID_HANDLE_VALUE));
    require(!safeCaseFlags(false,0) && !safeCaseFlags(true,FILE_CS_FLAG_CASE_SENSITIVE_DIR) && !safeCaseFlags(true,2));
    BY_HANDLE_FILE_INFORMATION knownAnchor{}, differentAnchor{}; differentAnchor.nFileIndexLow=42;
    require(isAnchor(root,root) && !sameIdentity(knownAnchor,differentAnchor));
    require(isAnchor(root,root) && !isAnchor(root.substr(0,root.find(L"Program Files")-1),root) && !isAnchor(root+L"\\Lab",root));
    for(const auto& child : {root,root+L"x\\test.exe",root+L"\\..\\test.exe",root+L"\\.\\test.exe",root+L"\\\\test.exe",root+L"\\test.exe:stream",std::wstring(L"relative\\test.exe")}) require(!contained(root,child));
    require(!acl(L"O:BUD:P(A;;FA;;;BA)"));
    require(!acl(L"O:BAD:(A;;FA;;;BA)"));
    require(!acl(L"O:BAD:PNO_ACCESS_CONTROL"));
    for(const wchar_t* rights : {L"FW",L"DC",L"WD",L"WO",L"SD",L"GA"}) {
        std::wstring s=L"O:BAD:P(A;;FA;;;BA)(A;;"; s+=rights; s+=L";;;BU)"; require(!acl(s.c_str()));
    }
    require(!acl(L"O:BAD:P(OA;;FR;00112233-4455-6677-8899-aabbccddeeff;;BU)"));
    BY_HANDLE_FILE_INFORMATION meta{}; meta.nNumberOfLinks=1;
    require(safeMetadata(meta,false)); auto replacement=meta; replacement.nFileIndexLow=1;
    require(!sameIdentity(meta,replacement)); replacement=meta; replacement.dwVolumeSerialNumber=1;
    require(!sameIdentity(meta,replacement)); meta.nNumberOfLinks=2; require(!safeMetadata(meta,false));
    meta.nNumberOfLinks=1; meta.dwFileAttributes=FILE_ATTRIBUTE_REPARSE_POINT; require(!safeMetadata(meta,false));
    meta.dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY; require(safeMetadata(meta,true) && !safeMetadata(meta,false));
    meta.dwFileAttributes|=FILE_ATTRIBUTE_REPARSE_POINT; require(!safeMetadata(meta,true));
    std::puts("MODEL: codec, stale IDs, counter, ACL, reparse, links and substitution identity cases passed");
    // This seam exercises the same write/flush routine, never protected admission.
    // The fixed owned build fixture is CREATE_NEW and cannot overwrite host state.
    wchar_t image[32768]; DWORD n=GetModuleFileNameW(nullptr,image,32768); require(n && n<32768);
    std::wstring fixture(image,n); fixture.resize(fixture.find_last_of(L'\\'));
    nativeCaseFixture(fixture);
    fixture+=L"\\lab-owned-io-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HANDLE h=CreateFileW(fixture.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(h!=INVALID_HANDLE_VALUE);
    require(!testWrite(h,r,Failure::ShortWrite)); LARGE_INTEGER size{}; require(GetFileSizeEx(h,&size) && size.QuadPart==47);
    require(!testWrite(h,r,Failure::Flush)); require(GetFileSizeEx(h,&size) && size.QuadPart==48);
    require(testWrite(h,r,Failure::None)); CloseHandle(h);
    require(DeleteFileW(fixture.c_str())!=FALSE);
    std::puts("MODEL injection: short write and flush failure preserve owned file; real write/flush passed");
    Directory directory;
    if(!directory.pin()) {
        require(!directory.create(r.installation,r.generation));
        require(!directory.acknowledge(r.installation,r.generation));
        std::puts("NATIVE: protected path unavailable; positive, stale-generation, reparse and substitution I/O not exercised");
    } else {
        // Never create native fixture automatically: this host task owns only its
        // ignored temporary I/O file, not an administrator-provisioned directory.
        std::puts("NATIVE: directory pinned; protected mutation not exercised");
    }
    return 0;
}

#include "lab-protected-file.h"
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <algorithm>
namespace labfile {
static bool nonzero(const Id& id) { return std::any_of(id.begin(), id.end(), [](unsigned char b) { return b != 0; }); }
bool encode(const Record& r, Bytes& b) {
    if (!nonzero(r.installation) || !nonzero(r.generation) || r.counter > 1024) return false;
    b = {}; b[0]='B'; b[1]='L'; b[2]='P'; b[3]='F'; b[4]=1; b[5]=1;
    std::copy(r.installation.begin(), r.installation.end(), b.begin()+8);
    std::copy(r.generation.begin(), r.generation.end(), b.begin()+24);
    for (unsigned n=0; n<4; ++n) b[40+n]=static_cast<unsigned char>(r.counter >> (8*n));
    return true;
}
bool decode(const unsigned char* p, size_t size, Record& result) {
    if (!p || size != 48) return false;
    Record r;
    std::copy(p+8,p+24,r.installation.begin()); std::copy(p+24,p+40,r.generation.begin());
    for (unsigned n=0;n<4;++n) r.counter |= static_cast<unsigned>(p[40+n]) << (8*n);
    Bytes canonical;
    if (!encode(r,canonical) || !std::equal(canonical.begin(),canonical.end(),p)) return false;
    result=r; return true;
}
bool advance(Record& r, const Id& i, const Id& g) {
    Bytes b; if (!encode(r,b) || r.installation!=i || r.generation!=g || r.counter>=1024) return false;
    ++r.counter; return true;
}
static bool trusted(PSID sid) {
    if (!sid || !IsValidSid(sid)) return false;
    PSID ti=nullptr;
    if (!ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464",&ti)) return false;
    bool ok=IsWellKnownSid(sid,WinLocalSystemSid) || IsWellKnownSid(sid,WinBuiltinAdministratorsSid) || EqualSid(sid,ti);
    LocalFree(ti); return ok;
}
bool safeDescriptor(PSECURITY_DESCRIPTOR sd) {
    PSID owner=nullptr; PACL acl=nullptr; BOOL present=FALSE, ignored=FALSE;
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision=0;
    if (!sd || !IsValidSecurityDescriptor(sd) || !GetSecurityDescriptorOwner(sd,&owner,&ignored) || !trusted(owner) ||
        !GetSecurityDescriptorControl(sd,&control,&revision) || !(control & SE_DACL_PROTECTED) ||
        !GetSecurityDescriptorDacl(sd,&present,&acl,&ignored) || !present || !acl || !IsValidAcl(acl)) return false;
    GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};
    for (DWORD n=0;n<acl->AceCount;++n) {
        void* raw=nullptr; if (!GetAce(acl,n,&raw)) return false;
        auto h=static_cast<ACE_HEADER*>(raw);
        // Unknown ACEs refuse even when inherit-only: no unsupported policy interpretation.
        if (h->AceType != ACCESS_ALLOWED_ACE_TYPE && h->AceType != ACCESS_DENIED_ACE_TYPE) return false;
        if (h->AceType == ACCESS_DENIED_ACE_TYPE || (h->AceFlags & INHERIT_ONLY_ACE)) continue;
        auto a=static_cast<ACCESS_ALLOWED_ACE*>(raw); DWORD mask=a->Mask; MapGenericMask(&mask,&mapping);
        constexpr DWORD mutation=FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES|FILE_DELETE_CHILD|DELETE|WRITE_DAC|WRITE_OWNER|GENERIC_ALL|GENERIC_WRITE|MAXIMUM_ALLOWED;
        if (!trusted(&a->SidStart) && (mask & mutation)) return false;
    }
    return true;
}
bool safeMetadata(const BY_HANDLE_FILE_INFORMATION& info, bool directory) {
    return !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
        !!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==directory && (directory || info.nNumberOfLinks==1);
}
bool sameIdentity(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber && a.nFileIndexHigh==b.nFileIndexHigh && a.nFileIndexLow==b.nFileIndexLow;
}
static bool secure(HANDLE h, bool directory, BY_HANDLE_FILE_INFORMATION& info, bool authority=true) {
    if (GetFileType(h)!=FILE_TYPE_DISK || !GetFileInformationByHandle(h,&info) ||
        !safeMetadata(info,directory)) return false;
    if (!authority) return true;
    PSECURITY_DESCRIPTOR sd=nullptr;
    DWORD e=GetSecurityInfo(h,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,nullptr,&sd);
    bool ok=e==ERROR_SUCCESS && safeDescriptor(sd); if(sd) LocalFree(sd); return ok;
}
Directory::~Directory() { for(HANDLE h:pins) CloseHandle(h); }
// Accept only canonical volume paths with ordinary, nonempty components.
static bool validPath(const std::wstring& p) {
    if (p.size()<50 || p.compare(0,11,L"\\\\?\\Volume{")!=0) return false;
    size_t root=p.find(L"}\\"); if(root==std::wstring::npos) return false;
    size_t pos=root+2;
    while(pos<p.size()) {
        size_t end=p.find(L'\\',pos); if(end==std::wstring::npos) end=p.size();
        std::wstring part=p.substr(pos,end-pos);
        if(part.empty() || part==L"." || part==L".." || part.find(L':')!=std::wstring::npos || part.back()==L'.' || part.back()==L' ') return false;
        pos=end+1;
    }
    return p.back()!=L'\\';
}
bool contained(const std::wstring& root,const std::wstring& child) {
    return validPath(root) && validPath(child) && child.size()>root.size()+1 &&
        child[root.size()]==L'\\' && CompareStringOrdinal(root.c_str(),static_cast<int>(root.size()),child.c_str(),static_cast<int>(root.size()),TRUE)==CSTR_EQUAL;
}
bool isAnchor(const std::wstring& component,const std::wstring& anchor) {
    return CompareStringOrdinal(component.c_str(),-1,anchor.c_str(),-1,TRUE)==CSTR_EQUAL;
}
static bool volumePath(const std::wstring& source,std::wstring& result) {
    if(source.size()<4 || source[1]!=L':' || source[2]!=L'\\' || GetDriveTypeW(source.substr(0,3).c_str())!=DRIVE_FIXED) return false;
    wchar_t volume[MAX_PATH];
    if(!GetVolumeNameForVolumeMountPointW(source.substr(0,3).c_str(),volume,MAX_PATH)) return false;
    result=std::wstring(volume)+source.substr(3); return validPath(result);
}
bool Directory::pin() {
    if (!pins.empty() || !path.empty()) return false;
    wchar_t image[32768]; DWORD length=GetModuleFileNameW(nullptr,image,32768);
    if (!length || length>=32768) return false;
    PWSTR known=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&known))) return false;
    std::wstring anchor, module;
    bool valid=volumePath(known,anchor) && volumePath(std::wstring(image,length),module);
    CoTaskMemFree(known);
    if(!valid || !contained(anchor,module)) return false;
    std::wstring candidate=module.substr(0,module.find_last_of(L'\\'))+L"\\LABProtectedFiles";
    if(!contained(anchor,candidate)) return false;
    size_t end=candidate.find(L"}\\")+2;
    for (;;) {
        std::wstring component=candidate.substr(0,end);
        HANDLE h=CreateFileW(component.c_str(),FILE_READ_ATTRIBUTES|READ_CONTROL,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(h==INVALID_HANDLE_VALUE) return false;
        pins.push_back(h);
        if(isAnchor(component,anchor)) authority=pins.size()-1;
        BY_HANDLE_FILE_INFORMATION info{};
        if(!secure(h,true,info,authority!=0)) return false;
        if(end==candidate.size()) break;
        end=candidate.find(L'\\',end+1); if(end==std::wstring::npos) end=candidate.size();
    }
    if(!authority) return false;
    path=candidate+L"\\pending.bin"; return true;
}
static bool writeRecord(HANDLE h,const Record& r, unsigned failure) {
    Bytes b; if(!encode(r,b)) return false;
    LARGE_INTEGER zero{}; if(!SetFilePointerEx(h,zero,nullptr,FILE_BEGIN)) return false;
    DWORD written=0; DWORD count=static_cast<DWORD>(b.size());
#ifdef LAB_PROTECTED_FILE_TESTING
    if(failure==1) --count;
#else
    (void)failure;
#endif
    if(!WriteFile(h,b.data(),count,&written,nullptr) || written!=b.size()) return false;
#ifdef LAB_PROTECTED_FILE_TESTING
    if(failure==2) return false;
#endif
    return FlushFileBuffers(h)!=FALSE;
}
bool Directory::operate(const Id& installation,const Id& generation,bool createNew) {
    Record r{installation,generation,0}; Bytes check;
    if(path.empty() || !encode(r,check)) return false;
    for(size_t n=0;n<pins.size();++n) { BY_HANDLE_FILE_INFORMATION i{}; if(!secure(pins[n],true,i,n>=authority)) return false; }
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(createNew && !ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;FA;;;SY)(A;;FA;;;BA)",SDDL_REVISION_1,&descriptor,nullptr)) return false;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|READ_CONTROL,0,createNew?&attributes:nullptr,
        createNew?CREATE_NEW:OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(descriptor) LocalFree(descriptor);
    if(h==INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION before{},after{}; bool ok=secure(h,false,before);
    if(ok && !createNew) {
        unsigned char bytes[49]; DWORD read=0;
        ok=before.nFileSizeHigh==0 && before.nFileSizeLow==48 && ReadFile(h,bytes,sizeof(bytes),&read,nullptr) &&
            decode(bytes,read,r) && advance(r,installation,generation);
    }
    ok=ok && secure(h,false,after) && sameIdentity(before,after);
    if(ok) ok=writeRecord(h,r,0);
    CloseHandle(h); return ok;
}
bool Directory::create(const Id& i,const Id& g) { return operate(i,g,true); }
bool Directory::acknowledge(const Id& i,const Id& g) { return operate(i,g,false); }
#ifdef LAB_PROTECTED_FILE_TESTING
bool testWrite(HANDLE h,const Record& r,Failure f) { return writeRecord(h,r,static_cast<unsigned>(f)); }
#endif
}

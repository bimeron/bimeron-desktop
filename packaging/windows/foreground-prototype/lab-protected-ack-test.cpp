#include "lab-protected-ack.cpp"
#include <cstdlib>
#include <fstream>
#include <iterator>
static void require(bool ok,int line) { if(!ok) { std::printf("FAIL %d error=%lu\n",line,GetLastError()); std::exit(1); } }
#define CHECK(b) require((b),__LINE__)
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==3);
    CHECK(labFreshEligibility(L"",INSTALLSTATE_UNKNOWN,true));
    CHECK(labFreshEligibility(L"ALL",INSTALLSTATE_UNKNOWN,true));
    for(const auto value:{L"all",L"All",L"ALL ",L"Other",L"ALL,Other"})
        CHECK(!labFreshEligibility(value,INSTALLSTATE_UNKNOWN,true));
    CHECK(!labFreshEligibility(L"ALL",INSTALLSTATE_DEFAULT,true));
    CHECK(!labFreshEligibility(L"ALL",INSTALLSTATE_UNKNOWN,false));
    Diagnostic first;
    CHECK(!refused(first,31,2,87)); CHECK(!refused(first,40,3,5));
    CHECK(first.stage==31 && first.detail==2 && first.status==87);
    std::ifstream input(argv[1],std::ios::binary);
    Buffer manifestBytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    labfile::Id installation{},generation{};
    CHECK(validManifest(manifestBytes,installation,generation));
    CHECK(!validManifest(Buffer(175),installation,generation));
    CHECK(!validManifest(Buffer(65537),installation,generation));
    for(size_t n:{size_t(0),size_t(40),size_t(44),size_t(144),size_t(176)}) {
        auto bad=manifestBytes; bad[n]^=0x80; CHECK(!validManifest(bad,installation,generation));
    }
    for(size_t n:{size_t(8),size_t(24),size_t(48),size_t(64),size_t(80),size_t(112)}) {
        auto bad=manifestBytes; std::fill(bad.begin()+n,bad.begin()+n+(n<80?16:32),static_cast<unsigned char>(0));
        CHECK(!validManifest(bad,installation,generation));
    }
    Msi databaseFile; CHECK(MsiOpenDatabaseW(argv[2],nullptr,&databaseFile.h)==ERROR_SUCCESS);
    CHECK(zeroResources(databaseFile.h));
    Buffer projection; CHECK(databaseHandle(databaseFile.h,projection));
    CHECK(projection.size()==manifestBytes.size()-176 && std::equal(projection.begin(),projection.end(),manifestBytes.begin()+176));
    CHECK(headerDatabase(databaseFile.h,manifestBytes));
    auto changed=projection; changed[0]^=1; Buffer differing; Diagnostic category;
    CHECK(!databaseHandle(databaseFile.h,differing,&category,&changed));
    CHECK(category.stage==41 && category.detail==1);
    for(size_t n:{size_t(48),size_t(64),size_t(112)}) {
        auto bad=manifestBytes; bad[n]^=1; CHECK(!headerDatabase(databaseFile.h,bad));
    }
    std::puts("NATIVE read-only database: C++ canonical projection matches Python manifest; GUID/Binary headers and malformed manifests checked; no installer session");
    Token observed; CHECK(openEffectiveToken(observed.h)==2);
    Token process,thread;
    CHECK(OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY,&process.h)!=FALSE);
    CHECK(DuplicateToken(process.h,SecurityImpersonation,&thread.h)!=FALSE);
    CHECK(SetThreadToken(nullptr,thread.h)!=FALSE);
    { Token selected; CHECK(openEffectiveToken(selected.h)==1);
      TOKEN_STATISTICS a{},b{}; DWORD n=0;
      CHECK(GetTokenInformation(selected.h,TokenStatistics,&a,sizeof(a),&n)!=FALSE);
      CHECK(GetTokenInformation(thread.h,TokenStatistics,&b,sizeof(b),&n)!=FALSE);
      CHECK(a.TokenId.LowPart==b.TokenId.LowPart && a.TokenId.HighPart==b.TokenId.HighPart); }
    CHECK(RevertToSelf()!=FALSE);
    Token anonymous; CHECK(DuplicateToken(process.h,SecurityAnonymous,&anonymous.h)!=FALSE);
    CHECK(SetThreadToken(nullptr,anonymous.h)!=FALSE);
    { Token denied; CHECK(openEffectiveToken(denied.h)==0 && !denied.h); }
    CHECK(RevertToSelf()!=FALSE);
    std::puts("NATIVE: actual callback token reader selected process then actual thread token; token IDs match");
    labfile::Record r; r.installation[0]=1; r.generation[0]=2;
    auto stale=r.generation; stale[1]=3;
    CHECK(!labfile::advanceOnce(r,r.installation,stale) && r.counter==0);
    wchar_t image[32768]; DWORD n=GetModuleFileNameW(nullptr,image,32768); CHECK(n && n<32768);
    std::wstring path(image,n); path.resize(path.find_last_of(L'\\'));
    path+=L"\\ack-owned-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(h!=INVALID_HANDLE_VALUE);
    HANDLE existing=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(existing==INVALID_HANDLE_VALUE && (GetLastError()==ERROR_FILE_EXISTS || GetLastError()==ERROR_SHARING_VIOLATION));
    CHECK(labfile::testWrite(h,r,labfile::Failure::None));
    CHECK(labfile::advanceOnce(r,r.installation,r.generation) && r.counter==1);
    CHECK(labfile::testWrite(h,r,labfile::Failure::None));
    CHECK(!labfile::advanceOnce(r,r.installation,r.generation) && r.counter==1);
    LARGE_INTEGER zero{}; CHECK(SetFilePointerEx(h,zero,nullptr,FILE_BEGIN)!=FALSE);
    labfile::Bytes bytes{}; DWORD read=0; CHECK(ReadFile(h,bytes.data(),48,&read,nullptr) && read==48);
    labfile::Record reread; CHECK(labfile::decode(bytes.data(),read,reread) && reread.counter==1);
    CHECK(!labfile::advanceOnce(reread,r.installation,r.generation) && reread.counter==1);
    CloseHandle(h); CHECK(DeleteFileW(path.c_str())!=FALSE);
    std::puts("NATIVE owned ignored file: full write/flush counter 0 and 1; shared exact-zero transition refused replay; not protected admission");
    std::puts("MODEL: stale generation refused; protected native/ACL/flush failure positives are not claimed");
    return 0;
}

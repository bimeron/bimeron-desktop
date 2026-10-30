#include "lab-protected-ack.cpp"
#include <cstdlib>
#include <fstream>
#include <iterator>
static void require(bool ok,int line) { if(!ok) { std::printf("FAIL %d error=%lu\n",line,GetLastError()); std::exit(1); } }
#define CHECK(b) require((b),__LINE__)
static void execute(MSIHANDLE db,const wchar_t* sql) {
    Msi view;
    CHECK(MsiDatabaseOpenViewW(db,sql,&view.h)==ERROR_SUCCESS);
    CHECK(MsiViewExecute(view.h,0)==ERROR_SUCCESS);
}
static void resourceQueryNegatives() {
    wchar_t image[32768]; DWORD n=GetModuleFileNameW(nullptr,image,32768); CHECK(n && n<32768);
    std::wstring path(image,n); path.resize(path.find_last_of(L'\\'));
    path+=L"\\ack-resource-query-"+std::to_wstring(GetCurrentProcessId())+L".msi";
    CHECK(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND);
    {
        Msi db; CHECK(MsiOpenDatabaseW(path.c_str(),reinterpret_cast<LPCWSTR>(MSIDBOPEN_CREATE),&db.h)==ERROR_SUCCESS);
        execute(db.h,L"CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL PRIMARY KEY `Component`)");
        execute(db.h,L"CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
        // File deliberately absent: native query failure must not mean empty.
        CHECK(!zeroResources(db.h));
        execute(db.h,L"CREATE TABLE `File` (`File` CHAR(72) NOT NULL PRIMARY KEY `File`)");
        CHECK(zeroResources(db.h));
        execute(db.h,L"INSERT INTO `Component` (`Component`) VALUES ('Unexpected')");
        CHECK(!zeroResources(db.h));
        execute(db.h,L"DELETE FROM `Component`");
        execute(db.h,L"INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Unexpected','Unexpected')");
        CHECK(!zeroResources(db.h));
        execute(db.h,L"DELETE FROM `FeatureComponents`");
        execute(db.h,L"INSERT INTO `File` (`File`) VALUES ('Unexpected')");
        CHECK(!zeroResources(db.h));
        execute(db.h,L"CREATE TABLE `PermanentProbe` (`Name` CHAR(72) NOT NULL PRIMARY KEY `Name`)");
        execute(db.h,L"CREATE TABLE `TemporaryProbe` (`Name` CHAR(72) NOT NULL TEMPORARY PRIMARY KEY `Name`) HOLD");
        CHECK(MsiDatabaseIsTablePersistentW(db.h,L"PermanentProbe")==MSICONDITION_TRUE);
        CHECK(MsiDatabaseIsTablePersistentW(db.h,L"TemporaryProbe")==MSICONDITION_FALSE);
        CHECK(MsiDatabaseIsTablePersistentW(db.h,L"AbsentProbe")==MSICONDITION_NONE);
        Buffer snapshot; CHECK(collectTables(db.h,snapshot));
        std::string report(snapshot.begin(),snapshot.end());
        CHECK(report.find("\"PermanentProbe\"\t1\n")!=std::string::npos);
        CHECK(report.find("\"TemporaryProbe\"\t0\n")!=std::string::npos);
        CHECK(MsiDatabaseCommit(db.h)==ERROR_SUCCESS);
    }
    CHECK(DeleteFileW(path.c_str())!=FALSE);
    std::puts("NATIVE owned MSI database: missing table and nonempty Component/FeatureComponents/File refused; no installer action");
}
static void projectionNegatives(const wchar_t* source,const Buffer& expected) {
    for(const auto name:{L"#_BaselineCost",L"#_BaselineData",L"#_BaselineFile",L"#_FolderCache",
        L"#_MsiRedirectedFile",L"#_PatchCache",L"CostAdjuster",L"_Property",L"__MsiPatchMedia"}) {
        CHECK(classifyOwner(name,MSICONDITION_FALSE)==Owner::Exclude);
        for(const auto state:{MSICONDITION_TRUE,MSICONDITION_NONE,MSICONDITION_ERROR})
            CHECK(classifyOwner(name,state)==Owner::Refuse);
    }
    for(const auto name:{L"Unknown",L"costadjuster",L"_Tables",L"_Columns",L"_Streams",L"_Storages"})
        for(const auto state:{MSICONDITION_FALSE,MSICONDITION_TRUE,MSICONDITION_NONE,MSICONDITION_ERROR})
            CHECK(classifyOwner(name,state)==Owner::Refuse);
    CHECK(classifyOwner(L"Property",MSICONDITION_TRUE)==Owner::Keep);
    for(const auto state:{MSICONDITION_FALSE,MSICONDITION_NONE,MSICONDITION_ERROR})
        CHECK(classifyOwner(L"Property",state)==Owner::Refuse);
    const wchar_t* mutations[]={nullptr,
        L"CREATE TABLE `CostAdjuster` (`Name` CHAR(72) NOT NULL TEMPORARY PRIMARY KEY `Name`) HOLD",
        L"CREATE TABLE `UnknownTemporary` (`Name` CHAR(72) NOT NULL TEMPORARY PRIMARY KEY `Name`) HOLD",
        L"CREATE TABLE `UnknownPersistent` (`Name` CHAR(72) NOT NULL PRIMARY KEY `Name`)",
        L"CREATE TABLE `CostAdjuster` (`Name` CHAR(72) NOT NULL PRIMARY KEY `Name`)",
        L"DROP TABLE `Component`",
        L"ALTER TABLE `Property` ADD `Unexpected` CHAR(20)"};
    for(unsigned k=0;k<sizeof(mutations)/sizeof(mutations[0]);++k) {
        std::wstring path=std::wstring(source)+L".owned-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(k)+L".msi";
        CHECK(CopyFileW(source,path.c_str(),TRUE)!=FALSE);
        {
            Msi db; CHECK(MsiOpenDatabaseW(path.c_str(),reinterpret_cast<LPCWSTR>(MSIDBOPEN_TRANSACT),&db.h)==ERROR_SUCCESS);
            if(mutations[k]) execute(db.h,mutations[k]);
            Buffer actual; Diagnostic d;
            const bool ok=databaseHandle(db.h,actual,&d,&expected);
            if(k<=1) { CHECK(ok && actual==expected); CHECK(!snapshotSelected(0,d)); }
            else if(k<=4) CHECK(!ok && d.stage==43);
            else CHECK(!ok && d.stage==41);
            if(k==1 || k==2 || k==4) {
                const wchar_t* owner=k==2 ? L"UnknownTemporary" : L"CostAdjuster";
                const auto persistence=MsiDatabaseIsTablePersistentW(db.h,owner);
                CHECK(persistence==(k==4 ? MSICONDITION_TRUE : MSICONDITION_FALSE));
                bool exclude=false; Diagnostic policy;
                CHECK(projectOwner(db.h,owner,exclude,&policy)==(k==1));
                CHECK(exclude==(k==1));
                // Some engines omit temporary columns from _Columns. Inspect actual
                // rows when present and test the same owner guard above regardless.
                Msi view; CHECK(MsiDatabaseOpenViewW(db.h,L"SELECT `Table` FROM `_Columns`",&view.h)==ERROR_SUCCESS);
                CHECK(MsiViewExecute(view.h,0)==ERROR_SUCCESS);
                unsigned visible=0;
                for(;;) {
                    Msi row; UINT e=MsiViewFetch(view.h,&row.h); if(e==ERROR_NO_MORE_ITEMS) break;
                    CHECK(e==ERROR_SUCCESS); std::wstring name; CHECK(field(row.h,1,name));
                    if(name==owner) ++visible;
                }
                Buffer columns; Diagnostic columnPolicy;
                CHECK(rows(db.h,L"_Columns",L"`Table`,`Number`,`Name`,`Type`",4,columns,&columnPolicy)==(k==1 || visible==0));
                if(k!=1 && visible) CHECK(columnPolicy.stage==43);
                std::printf("NATIVE _Columns owner case=%u visible=%u\n",k,visible);
            }
            CHECK(MsiDatabaseCommit(db.h)==ERROR_SUCCESS);
        }
        CHECK(DeleteFileW(path.c_str())!=FALSE);
    }
    std::puts("NATIVE owned MSI projection: baseline and temporary CostAdjuster exact; unknown/persisted collision/missing/schema negatives refused; classifier NONE/ERROR models");
}
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
    Diagnostic trigger; refused(trigger,41,1,87);
    CHECK(snapshotSelected(0,trigger));
    CHECK(!snapshotSelected(1,trigger) && !snapshotSelected(4,trigger));
    CHECK(!snapshotSelected(0,trigger,1) && !snapshotSelected(0,trigger,2));
    Diagnostic success,other; refused(other,41,2);
    CHECK(!snapshotSelected(0,success) && !snapshotSelected(0,other));
    Buffer escaped;
    CHECK(encodeTables({{L"z",MSICONDITION_ERROR},{L"a\t\n\\\"\u00e9",MSICONDITION_FALSE}},escaped));
    CHECK(std::string(escaped.begin(),escaped.end())=="BIMERON_ACK_ACTIVE_TABLES_V1\n\"a\\u0009\\u000A\\u005C\\u0022\\u00E9\"\t0\n\"z\"\t3\n");
    CHECK(!encodeTables(std::vector<std::pair<std::wstring,int>>(4097,{L"a",0}),escaped));
    CHECK(!encodeTables({{std::wstring(65536,L'\t'),0}},escaped));
    resourceQueryNegatives();
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
    projectionNegatives(argv[2],projection);
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
    Buffer diagnostic{'t','a','b','l','e','s','\n'};
    CHECK(labfile::testDiagnosticWrite(h,diagnostic));
    LARGE_INTEGER start{}; CHECK(SetFilePointerEx(h,start,nullptr,FILE_BEGIN));
    Buffer rereadDiagnostic(diagnostic.size()); DWORD diagnosticRead=0;
    CHECK(ReadFile(h,rereadDiagnostic.data(),static_cast<DWORD>(rereadDiagnostic.size()),&diagnosticRead,nullptr));
    CHECK(diagnosticRead==diagnostic.size() && rereadDiagnostic==diagnostic);
    CHECK(SetFilePointerEx(h,start,nullptr,FILE_BEGIN));
    CHECK(!labfile::testDiagnosticWrite(h,Buffer(65537)));
    CHECK(!labfile::testDiagnosticWrite(h,Buffer{}));
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

// Access-only lab. Never enrollment, initiating-client proof, or settlement.
#include "lab-protected-file.h"
#include <msi.h>
#include <propidl.h>
#include <msiquery.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#ifndef LAB_FAILPOINT
#define LAB_FAILPOINT 0
#endif
namespace {
using Buffer=std::vector<unsigned char>;
struct Msi { MSIHANDLE h=0; ~Msi(){if(h) MsiCloseHandle(h);} };
struct Token { HANDLE h=nullptr; ~Token(){if(h) CloseHandle(h);} };
void number(Buffer& b,unsigned n) { for(unsigned k=0;k<4;++k) b.push_back(static_cast<unsigned char>(n>>(8*k))); }
void text(Buffer& b,const std::wstring& s) {
    number(b,static_cast<unsigned>(s.size()*2));
    const auto p=reinterpret_cast<const unsigned char*>(s.data()); b.insert(b.end(),p,p+s.size()*2);
}
bool field(MSIHANDLE r,unsigned n,std::wstring& s) {
    wchar_t b[32768]; DWORD size=32768;
    if(MsiRecordGetStringW(r,n,b,&size)!=ERROR_SUCCESS) return false;
    s.assign(b,size); return true;
}
bool rows(MSIHANDLE db,const wchar_t* table,const wchar_t* columns,unsigned count,Buffer& out) {
    std::wstring sql=L"SELECT "; sql+=columns; sql+=L" FROM `"; sql+=table; sql+=L"`";
    Msi view; if(MsiDatabaseOpenViewW(db,sql.c_str(),&view.h)!=ERROR_SUCCESS || MsiViewExecute(view.h,0)!=ERROR_SUCCESS) return false;
    std::vector<std::vector<std::wstring>> values; size_t total=0;
    for(;;) {
        Msi record; UINT e=MsiViewFetch(view.h,&record.h); if(e==ERROR_NO_MORE_ITEMS) break;
        if(e!=ERROR_SUCCESS || values.size()>=4096) return false;
        std::vector<std::wstring> row;
        for(unsigned n=1;n<=count;++n) {
            std::wstring value;
            if(std::wstring(table)==L"Binary" && n==2) {
                Buffer binary; unsigned char chunk[65536];
                for(;;) { DWORD size=sizeof(chunk); if(MsiRecordReadStream(record.h,n,reinterpret_cast<char*>(chunk),&size)!=ERROR_SUCCESS) return false;
                    if(!size) break; if(binary.size()+size>16*1024*1024) return false; binary.insert(binary.end(),chunk,chunk+size); }
                unsigned char digest[32];
                if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,binary.data(),static_cast<ULONG>(binary.size()),digest,32)<0) return false;
                wchar_t hex[65]{}; for(unsigned k=0;k<32;++k) swprintf_s(hex+2*k,3,L"%02x",digest[k]); value=hex;
            } else if(!field(record.h,n,value)) return false;
            total+=value.size()*2+4; if(total>65536) return false;
            row.push_back(value);
        }
        values.push_back(row);
    }
    std::sort(values.begin(),values.end()); text(out,table); number(out,static_cast<unsigned>(values.size()));
    for(const auto& row:values) { number(out,count); for(const auto& value:row) text(out,value); }
    return out.size()<=65536;
}
// Same fixed canonical projection as the offline actual-database checker.
bool databaseHandle(MSIHANDLE db,Buffer& out) {
    if(!db) return false;
    struct Table { const wchar_t* name; const wchar_t* columns; unsigned count; };
    const Table tables[]={
        {L"_Tables",L"`Name`",1},{L"_Columns",L"`Table`,`Number`,`Name`,`Type`",4},
        {L"Component",L"`Component`,`ComponentId`,`Directory_`,`Attributes`,`Condition`,`KeyPath`",6},
        {L"FeatureComponents",L"`Feature_`,`Component_`",2},{L"File",L"`File`",1},
        {L"Media",L"`DiskId`,`LastSequence`,`DiskPrompt`,`Cabinet`,`VolumeLabel`,`Source`",6},
        {L"AdminExecuteSequence",L"`Action`,`Condition`,`Sequence`",3},{L"AdminUISequence",L"`Action`,`Condition`,`Sequence`",3},
        {L"AdvtExecuteSequence",L"`Action`,`Condition`,`Sequence`",3},{L"InstallUISequence",L"`Action`,`Condition`,`Sequence`",3},
        {L"InstallExecuteSequence",L"`Action`,`Condition`,`Sequence`",3},
        {L"CustomAction",L"`Action`,`Type`,`Source`,`Target`",4},{L"Binary",L"`Name`,`Data`",2},
        {L"Directory",L"`Directory`,`Directory_Parent`,`DefaultDir`",3},
        {L"Feature",L"`Feature`,`Feature_Parent`,`Title`,`Description`,`Display`,`Level`,`Directory_`,`Attributes`",8},
        {L"Property",L"`Property`,`Value`",2},
        {L"_Streams",L"`Name`",1},{L"_Storages",L"`Name`",1}};
    for(const auto& t:tables) if(!rows(db,t.name,t.columns,t.count,out)) return false;
    Msi summary;
    if(MsiGetSummaryInformationW(db,nullptr,0,&summary.h)!=ERROR_SUCCESS) return false;
    for(unsigned id:{7u,9u,15u}) {
        UINT type=0; INT integer=0; FILETIME time{}; wchar_t value[1024]; DWORD size=1024;
        if(MsiSummaryInfoGetPropertyW(summary.h,id,&type,&integer,&time,value,&size)!=ERROR_SUCCESS) return false;
        text(out,type==VT_LPSTR ? std::wstring(value,size) : std::to_wstring(integer));
    }
    return true;
}
bool database(MSIHANDLE install,Buffer& out) { Msi db; db.h=MsiGetActiveDatabase(install); return databaseHandle(db.h,out); }
bool emptyProperty(MSIHANDLE h,const wchar_t* name) {
    wchar_t b[2]{}; DWORD n=2; return MsiGetPropertyW(h,name,b,&n)==ERROR_SUCCESS && n==0;
}
bool fresh(MSIHANDLE h) {
    for(const auto name:{L"Installed",L"TRANSFORMS",L"PATCH",L"REINSTALL",L"REMOVE",L"ADDLOCAL",L"ADDSOURCE",L"ADVERTISE",L"MSINEWINSTANCE"})
        if(!emptyProperty(h,name)) return false;
    return MsiGetMode(h,MSIRUNMODE_ROLLBACKENABLED)!=FALSE;
}
DWORD openEffectiveToken(HANDLE& token) {
    if(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&token)) return 1;
    if(GetLastError()!=ERROR_NO_TOKEN) return 0;
    return OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token) ? 2 : 0;
}
bool effectiveToken() {
    Token token;
    if(!openEffectiveToken(token.h)) return false;
    TOKEN_ELEVATION elevation{}; DWORD n=0,restricted=1;
    if(!GetTokenInformation(token.h,TokenElevation,&elevation,sizeof(elevation),&n) || !elevation.TokenIsElevated ||
       !GetTokenInformation(token.h,TokenHasRestrictions,&restricted,sizeof(restricted),&n) || restricted) return false;
    return true; // Actual protected file opens additionally enforce this effective token's access.
}
bool validManifest(const Buffer& b,labfile::Id& i,labfile::Id& g) {
    if(b.size()<=176 || b.size()>65536 ||
        std::memcmp(b.data(),"BLACK001",8)!=0) return false;
    std::copy(b.begin()+8,b.begin()+24,i.begin()); std::copy(b.begin()+24,b.begin()+40,g.begin());
    labfile::Bytes record; if(!labfile::encode({i,g,0},record)) return false;
#ifdef _M_ARM64
    constexpr unsigned arch=2;
#else
    constexpr unsigned arch=1;
#endif
    if(b[40]!=arch || b[41] || b[42] || b[43] || b[44]!=LAB_FAILPOINT || b[45] || b[46] || b[47]) return false;
    for(unsigned start:{48u,64u,80u,112u,144u}) {
        const unsigned length=start<80?16:32;
        if(std::all_of(b.begin()+start,b.begin()+start+length,[](unsigned char c){return c==0;})) return false;
    }
    unsigned char digest[32]{};
    return BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(b.data()+176),static_cast<ULONG>(b.size()-176),digest,32)>=0 &&
        std::equal(digest,digest+32,b.begin()+144);
}
bool manifest(labfile::Directory& dir,Buffer& b,labfile::Id& i,labfile::Id& g) {
    return dir.pinAck() && dir.readAckManifest(b) && validManifest(b,i,g);
}
bool headerDatabase(MSIHANDLE db,const Buffer& b) {
    if(!db) return false;
    Msi view;
    if(MsiDatabaseOpenViewW(db,L"SELECT `Value` FROM `Property` WHERE `Property`='ProductCode'",&view.h)!=ERROR_SUCCESS ||
        MsiViewExecute(view.h,0)!=ERROR_SUCCESS) return false;
    Msi row; std::wstring product;
    if(MsiViewFetch(view.h,&row.h)!=ERROR_SUCCESS || !field(row.h,1,product)) return false;
    GUID expected{}; std::memcpy(&expected,b.data()+48,16); wchar_t guid[40]{};
    if(!StringFromGUID2(expected,guid,40) || product!=guid) return false;
    Msi summary; if(MsiGetSummaryInformationW(db,nullptr,0,&summary.h)!=ERROR_SUCCESS) return false;
    UINT type=0; INT integer=0; FILETIME time{}; wchar_t value[1024]; DWORD size=1024;
    if(MsiSummaryInfoGetPropertyW(summary.h,9,&type,&integer,&time,value,&size)!=ERROR_SUCCESS || type!=VT_LPSTR) return false;
    std::memcpy(&expected,b.data()+64,16);
    if(!StringFromGUID2(expected,guid,40) || std::wstring(value,size)!=guid) return false;
    Buffer actual,expectedBinary; if(!rows(db,L"Binary",L"`Name`,`Data`",2,actual)) return false;
    text(expectedBinary,L"Binary"); number(expectedBinary,1); number(expectedBinary,2); text(expectedBinary,L"LabAckBinary");
    wchar_t hex[65]{}; for(unsigned k=0;k<32;++k) swprintf_s(hex+2*k,3,L"%02x",b[112+k]); text(expectedBinary,hex);
    return actual==expectedBinary;
}
bool headerMatches(MSIHANDLE install,const Buffer& b) { Msi db; db.h=MsiGetActiveDatabase(install); return headerDatabase(db.h,b); }
bool logical(MSIHANDLE h,const Buffer& b) {
    unsigned char digest[32]{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(b.data()+176),static_cast<ULONG>(b.size()-176),digest,32)<0 || !std::equal(digest,digest+32,b.begin()+144)) return false;
    Buffer actual; return fresh(h) && headerMatches(h,b) && database(h,actual) && actual.size()==b.size()-176 &&
        std::equal(actual.begin(),actual.end(),b.begin()+176);
}
UINT action(MSIHANDLE h,unsigned point) {
    if(point==3) return LAB_FAILPOINT==1 ? ERROR_INSTALL_FAILURE : ERROR_SUCCESS;
    if(point==1) return ERROR_SUCCESS; // Rollback never changes pending state.
    labfile::Directory dir; Buffer b; labfile::Id i{},g{};
    if(!manifest(dir,b,i,g)) return ERROR_INSTALL_FAILURE;
    if(point==0) return logical(h,b) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    if(point==2) return dir.create(i,g) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    if(LAB_FAILPOINT==2 || !effectiveToken() || !logical(h,b)) return ERROR_INSTALL_FAILURE;
    return dir.acknowledgeOnce(i,g) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}
UINT safe(MSIHANDLE h,unsigned point) noexcept {
    UINT result=ERROR_INSTALL_FAILURE;
    try { result=action(h,point); } catch(...) {}
    Msi record; record.h=MsiCreateRecord(0); wchar_t message[160]{};
    swprintf_s(message,L"BIMERON_PROTECTED_ACK_V1 callback=%u result=%u initiatingClientUnknown=1 accessOnly=1",point,result);
    if(!record.h || MsiRecordSetStringW(record.h,0,message)!=ERROR_SUCCESS ||
        MsiProcessMessage(h,INSTALLMESSAGE_INFO,record.h)!=0) return ERROR_INSTALL_FAILURE;
    return result;
}
}
#define ACK_CALLBACK(name, point) extern "C" __declspec(dllexport) UINT __stdcall name(MSIHANDLE h) { return safe(h,point); }
ACK_CALLBACK(AckPreflight,0)
ACK_CALLBACK(AckRollback,1)
ACK_CALLBACK(AckDeferred,2)
ACK_CALLBACK(AckCommit,3)
ACK_CALLBACK(AckPostFinalize,4)

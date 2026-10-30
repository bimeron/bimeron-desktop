#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <bcrypt.h>
#include <aclapi.h>
#pragma comment(lib,"advapi32.lib")
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <string>
#include "pins.h"
std::string hash(const std::filesystem::path& p){
 std::ifstream f(p,std::ios::binary);if(!f)return "";
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char digest[32];
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return "";
 if(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(a,0);return "";}
 char b[65536];bool ok=true;while(f){f.read(b,sizeof b);if(BCryptHashData(h,(PUCHAR)b,(ULONG)f.gcount(),0)<0)ok=false;}
 ok=ok&&f.eof()&&BCryptFinishHash(h,digest,32,0)>=0;BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);
 if(!ok)return "";std::string out;const char* hex="0123456789abcdef";for(auto v:digest){out+=hex[v>>4];out+=hex[v&15];}return out;
}
// Pin the fixed directory and its parent against replacement for the entire run.
bool privateObject(HANDLE h,bool directory){
 BY_HANDLE_FILE_INFORMATION info{};
 if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0)!=directory)return false;
 PSECURITY_DESCRIPTOR sd=nullptr;PSID owner=nullptr;PACL acl=nullptr;
 if(GetSecurityInfo(h,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&sd)!=ERROR_SUCCESS)return false;
 BYTE system[SECURITY_MAX_SID_SIZE],admins[SECURITY_MAX_SID_SIZE];DWORD ns=sizeof system,na=sizeof admins;SECURITY_DESCRIPTOR_CONTROL control;DWORD revision;
 bool ok=CreateWellKnownSid(WinLocalSystemSid,nullptr,system,&ns)&&CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,admins,&na)&&
  (EqualSid(owner,system)||EqualSid(owner,admins))&&acl&&GetSecurityDescriptorControl(sd,&control,&revision)&&(!directory||(control&SE_DACL_PROTECTED));
 for(DWORD i=0;ok&&i<acl->AceCount;++i){void* ace=nullptr;if(!GetAce(acl,i,&ace)){ok=false;break;}
  auto header=(ACE_HEADER*)ace;
  if(header->AceType==ACCESS_ALLOWED_ACE_TYPE){auto allowed=(ACCESS_ALLOWED_ACE*)ace;PSID sid=&allowed->SidStart;ok=EqualSid(sid,system)||EqualSid(sid,admins);}
  else if(header->AceType!=ACCESS_DENIED_ACE_TYPE)ok=false;
 }
 LocalFree(sd);return ok;
}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 bool verify=wcscmp(argv[1],L"--verify-only")==0;
 bool restricted=wcscmp(argv[1],L"--guest-cost-initialize")==0;
 bool logged=wcscmp(argv[1],L"--guest-cost-initialize-machine-state-logged")==0;
 bool machine=wcscmp(argv[1],L"--guest-cost-initialize-machine-state")==0;
 if(!verify&&!restricted&&!machine&&!logged)return 2;
 wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n==32768)return 3;
 auto root=std::filesystem::path(exe).parent_path();
 // Hold read-only, non-write/non-delete-shared handles through all sessions.
 struct Files { HANDLE h[4]={}; ~Files(){for(auto v:h)if(v && v!=INVALID_HANDLE_VALUE)CloseHandle(v);} } files;
 for(int i=0;i<4;++i){files.h[i]=CreateFileW((root/names[i]).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(files.h[i]==INVALID_HANDLE_VALUE)return 4;}
 // Validate every fixed case before opening any installer session.
 for(int i=0;i<4;++i)if(hash(root/names[i])!=pins[i]){printf("{\"case\":%d,\"validation\":1}\n",i);return 4;}
 if(verify){puts("{\"validation\":0}");return 0;}
 struct Directories { HANDLE parent=INVALID_HANDLE_VALUE,dir=INVALID_HANDLE_VALUE; ~Directories(){if(dir!=INVALID_HANDLE_VALUE)CloseHandle(dir);if(parent!=INVALID_HANDLE_VALUE)CloseHandle(parent);} } dirs;
 const wchar_t* logRoot=L"C:\\ProgramData\\BimeronCostingLab-66bb4b28";
 if(logged){
  dirs.parent=CreateFileW(L"C:\\ProgramData",FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  BY_HANDLE_FILE_INFORMATION info{};
  if(dirs.parent==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(dirs.parent,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return 6;
  dirs.dir=CreateFileW(logRoot,READ_CONTROL|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  if(dirs.dir==INVALID_HANDLE_VALUE||!privateObject(dirs.dir,true))return 6;
 }
 printf("{\"mode\":\"%s\",\"options\":%u}\n",logged?"machine-state-logged":machine?"machine-state":"restricted",restricted?1u:0u);
 HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 printf("{\"com\":%lu}\n",(unsigned long)co);if(FAILED(co))return 5;
 for(int i=0;i<4;++i){MSIHANDLE h=0;auto p=root/names[i];
  if(logged){
   auto log=std::filesystem::path(logRoot)/(L"costing-engine-case"+std::to_wstring(i)+L".log");
   HANDLE reservation=CreateFileW(log.c_str(),GENERIC_WRITE|READ_CONTROL,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
   if(reservation==INVALID_HANDLE_VALUE){printf("{\"case\":%d,\"log_reservation\":%lu}\n",i,GetLastError());CoUninitialize();return 6;}
   if(!privateObject(reservation,false)){CloseHandle(reservation);CoUninitialize();return 6;}
   CloseHandle(reservation);
   UINT enabled=MsiEnableLogW(INSTALLLOGMODE_VERBOSE|INSTALLLOGMODE_EXTRADEBUG,log.c_str(),INSTALLLOGATTRIBUTES_FLUSHEACHLINE);
   printf("{\"case\":%d,\"log_enable\":%u}\n",i,enabled);
   if(enabled!=ERROR_SUCCESS){CoUninitialize();return 6;}
  }
  UINT opened=(machine||logged)?MsiOpenPackageExW(p.c_str(),0,&h):MsiOpenPackageExW(p.c_str(),MSIOPENPACKAGEFLAGS_IGNOREMACHINESTATE,&h);
  UINT action=ERROR_FUNCTION_NOT_CALLED,closed=ERROR_INVALID_HANDLE;
  if(opened==ERROR_SUCCESS){action=MsiDoActionW(h,L"CostInitialize");closed=MsiCloseHandle(h);}
  UINT disabled=logged?MsiEnableLogW(0,nullptr,0):ERROR_SUCCESS;
  if(logged)printf("{\"case\":%d,\"log_disable\":%u}\n",i,disabled);
  printf("{\"case\":%d,\"sha256\":\"%s\",\"open\":%u,\"action\":%u,\"close\":%u}\n",i,pins[i],opened,action,closed);
  if(disabled!=ERROR_SUCCESS){CoUninitialize();return 6;}
 }
 CoUninitialize();return 0;
}

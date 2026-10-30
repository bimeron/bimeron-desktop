#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <bcrypt.h>
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
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 bool verify=wcscmp(argv[1],L"--verify-only")==0;
 if(!verify&&wcscmp(argv[1],L"--guest-cost-initialize")!=0)return 2;
 wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n==32768)return 3;
 auto root=std::filesystem::path(exe).parent_path();
 // Hold read-only, non-write/non-delete-shared handles through all sessions.
 struct Files { HANDLE h[4]={}; ~Files(){for(auto v:h)if(v && v!=INVALID_HANDLE_VALUE)CloseHandle(v);} } files;
 for(int i=0;i<4;++i){files.h[i]=CreateFileW((root/names[i]).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(files.h[i]==INVALID_HANDLE_VALUE)return 4;}
 // Validate every fixed case before opening any installer session.
 for(int i=0;i<4;++i)if(hash(root/names[i])!=pins[i]){printf("{\"case\":%d,\"validation\":1}\n",i);return 4;}
 if(verify){puts("{\"validation\":0}");return 0;}
 HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 printf("{\"com\":%lu}\n",(unsigned long)co);if(FAILED(co))return 5;
 for(int i=0;i<4;++i){MSIHANDLE h=0;auto p=root/names[i];
  UINT opened=MsiOpenPackageExW(p.c_str(),MSIOPENPACKAGEFLAGS_IGNOREMACHINESTATE,&h);
  UINT action=ERROR_FUNCTION_NOT_CALLED,closed=ERROR_INVALID_HANDLE;
  if(opened==ERROR_SUCCESS){action=MsiDoActionW(h,L"CostInitialize");closed=MsiCloseHandle(h);}
  printf("{\"case\":%d,\"sha256\":\"%s\",\"open\":%u,\"action\":%u,\"close\":%u}\n",i,pins[i],opened,action,closed);
 }
 CoUninitialize();return 0;
}

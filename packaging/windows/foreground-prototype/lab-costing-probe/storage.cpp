#include <windows.h>
#include <objbase.h>
#include <cstdio>
#include <vector>
int wmain(int argc, wchar_t** argv) {
 if(argc!=3 && argc!=4) return 2;
 HRESULT hr=CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); if(FAILED(hr)) return 3;
 IStorage* s=nullptr;
 hr=StgOpenStorage(argv[2],nullptr,(argc==4?STGM_READWRITE:STGM_READ)|STGM_SHARE_EXCLUSIVE,nullptr,0,&s);
 if(FAILED(hr)){printf("error %lu\n",(unsigned long)hr);CoUninitialize();return 4;}
 if(argc==4 && wcscmp(argv[1],L"zero")==0){
  IStream* stream=nullptr;
  hr=s->CreateStream(argv[3],STGM_WRITE|STGM_SHARE_EXCLUSIVE,0,0,&stream);
  if(SUCCEEDED(hr)){ULARGE_INTEGER z{};hr=stream->SetSize(z);stream->Release();}
  if(SUCCEEDED(hr)) hr=s->Commit(STGC_DEFAULT);
 } else if(argc==3 && wcscmp(argv[1],L"dump")==0){
  IEnumSTATSTG* e=nullptr;hr=s->EnumElements(0,nullptr,0,&e);
  if(SUCCEEDED(hr)) {STATSTG st{};ULONG n;
   while((hr=e->Next(1,&st,&n))==S_OK){
    if(st.type!=STGTY_STREAM || st.cbSize.QuadPart>16777216){hr=E_FAIL;CoTaskMemFree(st.pwcsName);break;}
    for(auto p=st.pwcsName;*p;++p) printf("%04x",(unsigned)*p);printf(":");
    IStream* stream=nullptr;hr=s->OpenStream(st.pwcsName,nullptr,STGM_READ|STGM_SHARE_EXCLUSIVE,0,&stream);
    CoTaskMemFree(st.pwcsName);if(FAILED(hr))break;
    std::vector<unsigned char> b(st.cbSize.LowPart);ULONG got=0;hr=stream->Read(b.data(),(ULONG)b.size(),&got);stream->Release();
    if(FAILED(hr)||got!=b.size()){hr=E_FAIL;break;}for(auto v:b)printf("%02x",v);printf("\n");
   } e->Release();
  }
 } else hr=E_INVALIDARG;
 s->Release();CoUninitialize();return FAILED(hr)?5:0;
}

#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include "GhostingExpected.h"
extern "C" __declspec(dllimport) unsigned __stdcall GTAXeSSGhostingRuntimeApiVersion();
extern "C" __declspec(dllimport) const char* __stdcall GTAXeSSGhostingRuntimeContract();
static HANDLE pinnedGhostingFile=INVALID_HANDLE_VALUE;
static bool verifyGhosting(){
 if(GTAXeSSGhostingRuntimeApiVersion()!=GHOSTING_EXPECTED_API)return false;
 const char*contract=GTAXeSSGhostingRuntimeContract();if(!contract||std::strcmp(contract,GHOSTING_EXPECTED_CONTRACT))return false;
 HMODULE module=GetModuleHandleW(L"GTAXeSS.GhostingFix.dll");wchar_t path[32768]{};
 const DWORD count=module?GetModuleFileNameW(module,path,32768):0;if(!count||count>=32768)return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};unsigned char digest[32]{},buffer[65536]{};DWORD read{};
 bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 while(ok){if(!ReadFile(file,buffer,sizeof(buffer),&read,nullptr)){ok=false;break;}if(!read)break;ok=BCryptHashData(hash,buffer,read,0)>=0;}
 if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
 char actual[65]{};const char*hex="0123456789abcdef";for(unsigned i=0;i<32;++i){actual[i*2]=hex[digest[i]>>4];actual[i*2+1]=hex[digest[i]&15];}
 ok=ok&&std::strcmp(actual,GHOSTING_EXPECTED_SHA256)==0;
 if(ok)pinnedGhostingFile=file;else CloseHandle(file);return ok;
}
// Called by the existing compatibility handshake, outside Windows' loader lock.
extern "C" BOOL WINAPI GhostingVerifyRuntime(){
 static const bool verified=verifyGhosting();
 if(!verified)OutputDebugStringA("B531: GhostingFix version/interface/SHA256 mismatch; activation refused.\n");
 return verified?TRUE:FALSE;
}

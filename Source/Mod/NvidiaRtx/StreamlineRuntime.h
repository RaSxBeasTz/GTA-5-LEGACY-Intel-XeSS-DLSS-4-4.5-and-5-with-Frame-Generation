#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <wintrust.h>
#include <softpub.h>
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <string>
#include <stdexcept>
#include <fstream>
#pragma comment(lib,"wintrust.lib")
namespace rtx {
using Microsoft::WRL::ComPtr;
inline void check(HRESULT hr,const char* where){if(FAILED(hr))throw std::runtime_error(std::string(where)+": "+std::to_string((long)hr));}
inline void check(sl::Result result,const char* where){if(result!=sl::Result::eOk)throw std::runtime_error(std::string(where)+": SL "+std::to_string((int)result));}
inline bool signedModule(const std::wstring& path){WINTRUST_FILE_INFO file{sizeof(file)};file.pcwszFilePath=path.c_str();WINTRUST_DATA data{sizeof(data)};data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;auto result=WinVerifyTrust(nullptr,&policy,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);return result==ERROR_SUCCESS;}
struct Streamline {
 HMODULE dll{};bool initialized{};std::wstring directory,logs;
 PFun_slInit*init{};PFun_slShutdown*shutdown{};PFun_slSetD3DDevice*setDevice{};PFun_slIsFeatureSupported*supported{};PFun_slGetFeatureFunction*feature{};PFun_slUpgradeInterface*upgrade{};PFun_slGetNativeInterface*native{};PFun_slGetNewFrameToken*token{};PFun_slSetConstants*constants{};PFun_slSetTagForFrame*tags{};PFun_slEvaluateFeature*evaluate{};
 PFun_slDLSSGGetState*fgState{};PFun_slDLSSGSetOptions*fgOptions{};PFun_slReflexSetOptions*reflexOptions{};PFun_slReflexSleep*sleep{};PFun_slPCLSetMarker*marker{};PFun_slDLSSSetOptions*dlssOptions{};PFun_slDLSSGetOptimalSettings*optimal{};
 template<class T>void load(T*&fn,const char*name){fn=reinterpret_cast<T*>(GetProcAddress(dll,name));if(!fn)throw std::runtime_error(std::string("Missing SDK API: ")+name);}
 template<class T>void get(T*&fn,sl::Feature id,const char*name){void*p{};check(feature(id,name,p),name);fn=reinterpret_cast<T*>(p);}
 void start(const std::wstring& folder){
  directory=folder;auto path=folder+L"\\sl.interposer.dll";if(!signedModule(path))throw std::runtime_error("NVIDIA interposer signature validation failed");dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!dll)throw std::runtime_error("Cannot load NVIDIA Streamline");
  load(init,"slInit");load(shutdown,"slShutdown");load(setDevice,"slSetD3DDevice");load(supported,"slIsFeatureSupported");load(feature,"slGetFeatureFunction");load(upgrade,"slUpgradeInterface");load(native,"slGetNativeInterface");load(token,"slGetNewFrameToken");load(constants,"slSetConstants");load(tags,"slSetTagForFrame");load(evaluate,"slEvaluateFeature");
  sl::Feature features[]{sl::kFeatureDLSS,sl::kFeatureDLSS_G,sl::kFeatureReflex,sl::kFeaturePCL};const wchar_t*paths[]{directory.c_str()};
  sl::Preferences pref{};pref.featuresToLoad=features;pref.numFeaturesToLoad=4;pref.pathsToPlugins=paths;pref.numPathsToPlugins=1;pref.renderAPI=sl::RenderAPI::eD3D12;pref.showConsole=false;pref.engine=sl::EngineType::eCustom;pref.engineVersion="Zink-GTA-Legacy-R623";pref.projectId="1a33c2b8-2edb-4a0e-8a64-32d7eaf23fd7";pref.flags=sl::PreferenceFlags::eUseManualHooking|sl::PreferenceFlags::eUseFrameBasedResourceTagging|sl::PreferenceFlags::eDisableCLStateTracking;
  logs=folder+L"\\logs";CreateDirectoryW(logs.c_str(),nullptr);pref.pathToLogsAndData=logs.c_str();check(init(pref,sl::kSDKVersion),"slInit");initialized=true;
 }
 void bind(ID3D12Device* device){check(setDevice(device),"slSetD3DDevice");get(fgState,sl::kFeatureDLSS_G,"slDLSSGGetState");get(fgOptions,sl::kFeatureDLSS_G,"slDLSSGSetOptions");get(reflexOptions,sl::kFeatureReflex,"slReflexSetOptions");get(sleep,sl::kFeatureReflex,"slReflexSleep");get(marker,sl::kFeaturePCL,"slPCLSetMarker");get(dlssOptions,sl::kFeatureDLSS,"slDLSSSetOptions");get(optimal,sl::kFeatureDLSS,"slDLSSGetOptimalSettings");}
 void stop(){if(initialized){shutdown();initialized=false;}/* Keep DLL loaded: no unload while SDK/driver callbacks could still run. */}
};
}

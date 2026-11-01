#pragma once
#include <windows.h>
#include <string>
namespace gta {
struct Resolution { unsigned width,height; };
inline constexpr bool SupportedResolution(unsigned w,unsigned h){return (w==1920&&h==1080)||(w==2560&&h==1440)||(w==3840&&h==2160);}
inline constexpr bool SupportedCaptureExtent(unsigned w,unsigned h){return w>=640&&h>=360&&w<=3840&&h<=2160;}
inline Resolution ParseResolution(std::string mode,Resolution automatic={1920,1080}){
 for(auto&c:mode)if(c>='a'&&c<='z')c=char(c-'a'+'A');
 if(mode=="1080P"||mode=="1920X1080")return {1920,1080};
 if(mode=="1440P"||mode=="2560X1440")return {2560,1440};
 if(mode=="4K"||mode=="2160P"||mode=="3840X2160")return {3840,2160};
 return SupportedResolution(automatic.width,automatic.height)?automatic:Resolution{1920,1080};
}
inline Resolution ConfiguredInput(){
 static const Resolution size=[](){char exe[32768]{};GetModuleFileNameA(nullptr,exe,32768);std::string path(exe);path=path.substr(0,path.find_last_of("\\/")+1)+"GTAXeSS.ini";char mode[32]{};GetPrivateProfileStringA("XeSS","OutputResolution","1080P",mode,32,path.c_str());auto output=ParseResolution(mode);unsigned w=GetPrivateProfileIntA("Upscaling","InputWidth",output.width,path.c_str()),h=GetPrivateProfileIntA("Upscaling","InputHeight",output.height,path.c_str());return SupportedCaptureExtent(w,h)&&w<=output.width&&h<=output.height?Resolution{w,h}:output;}();return size;
}
inline bool ConfiguredInputMatches(unsigned w,unsigned h){auto input=ConfiguredInput();return w==input.width&&h==input.height;}
inline bool UpscalingEnabled(){static const bool enabled=[](){char exe[32768]{};GetModuleFileNameA(nullptr,exe,32768);std::string path(exe);path=path.substr(0,path.find_last_of("\\/")+1)+"GTAXeSS.ini";char mode[40]{};GetPrivateProfileStringA("Upscaling","Quality","NATIVE",mode,40,path.c_str());return _stricmp(mode,"NATIVE")!=0;}();return enabled;}
}

#include "../B25DepthHelper/RuntimePreferences.h"
#include "../ResolutionPolicy.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(){try{
 wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);auto root=std::filesystem::path(exe).parent_path();auto ini=root/L"GTAXeSS.ini",temp=root/L"GTAXeSS.tmp";
 auto write=[&](bool hud){std::ofstream out(temp);out<<"[XeSS]\nOutputResolution=1440P\n[Upscaling]\nQuality=QUALITY\nInputWidth=1280\nInputHeight=720\n[HUD]\nEnabled="<<(hud?1:0)<<"\nShowFPS="<<(hud?0:1)<<"\nShowFrameTimes="<<(hud?0:1)<<"\nShowUsage=0\nShowStatus=1\n";out.close();if(!MoveFileExW(temp.c_str(),ini.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("atomic replacement");};
 write(false);auto first=RuntimePreferences::get();if(first.hud||!first.fps||!first.frameTimes||first.usage||!first.status)throw std::runtime_error("initial HUD read");write(true);Sleep(300);auto live=RuntimePreferences::get();if(!live.hud||live.fps||live.frameTimes||live.usage||!live.status)throw std::runtime_error("HUD did not refresh after external file replacement");
 if(!gta::ConfiguredInputMatches(1280,720)||gta::ConfiguredInputMatches(2560,1440)||!gta::UpscalingEnabled())throw std::runtime_error("upscaling source gate");
 std::cout<<"PASS: live HUD reload after atomic INI replacement; SDK input dimensions accepted independently of output dimensions.\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}

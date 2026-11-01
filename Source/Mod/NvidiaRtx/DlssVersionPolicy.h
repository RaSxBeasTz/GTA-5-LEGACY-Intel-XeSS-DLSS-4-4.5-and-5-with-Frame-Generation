#pragma once
// DLSS version labels select explicit SR models, not different DLL filenames.
// DLSS 5 combines the 4.5 SR model with the separate Neural Rendering pass.
namespace rtx {
inline sl::DLSSPreset versionPreset(const wchar_t* version,sl::DLSSMode mode){
 if(wcscmp(version,L"4")==0)return sl::DLSSPreset::ePresetK;
 return mode==sl::DLSSMode::eUltraPerformance?sl::DLSSPreset::ePresetL:sl::DLSSPreset::ePresetM;
}
inline void setVersionPresets(sl::DLSSOptions&options,const wchar_t*version){
 auto normal=versionPreset(version,sl::DLSSMode::eMaxQuality);
 options.dlaaPreset=options.qualityPreset=options.balancedPreset=options.performancePreset=options.ultraQualityPreset=normal;
 options.ultraPerformancePreset=versionPreset(version,sl::DLSSMode::eUltraPerformance);
}
}

#pragma once
#include <dxgiformat.h>
namespace gta {
inline constexpr bool SupportedDepthFormat(DXGI_FORMAT format){return format==DXGI_FORMAT_R24G8_TYPELESS||format==DXGI_FORMAT_R32G8X24_TYPELESS;}
inline constexpr DXGI_FORMAT DepthSrvFormat(DXGI_FORMAT format){return format==DXGI_FORMAT_R32G8X24_TYPELESS?DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:DXGI_FORMAT_R24_UNORM_X8_TYPELESS;}
}

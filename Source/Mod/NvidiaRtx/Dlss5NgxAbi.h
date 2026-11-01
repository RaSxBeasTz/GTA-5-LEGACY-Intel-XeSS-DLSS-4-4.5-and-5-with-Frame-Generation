// ABI declarations adapted from DLSS5ForUE5. See DLSS5-reference-LICENSE.txt.
#pragma once

#include <cstdint>
using uint32=uint32_t;using int32=int32_t;

struct ID3D11Resource;
struct ID3D12Resource;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

using FNGXResult = uint32;
using FNGXFeature = int32;

struct FNGXHandle
{
    uint32 Id;
};

// ABI-compatible subset of NVSDK_NGX_Parameter from NVIDIA's public NGX interface.
// IMPORTANT: vtable method order must stay exactly as declared here.
struct FNGXParameter
{
    virtual void Set(const char* InName, unsigned long long InValue) = 0;
    virtual void Set(const char* InName, float InValue) = 0;
    virtual void Set(const char* InName, double InValue) = 0;
    virtual void Set(const char* InName, unsigned int InValue) = 0;
    virtual void Set(const char* InName, int InValue) = 0;
    virtual void Set(const char* InName, ID3D11Resource* InValue) = 0;
    virtual void Set(const char* InName, ID3D12Resource* InValue) = 0;
    virtual void Set(const char* InName, void* InValue) = 0;

    virtual FNGXResult Get(const char* InName, unsigned long long* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, float* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, double* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, unsigned int* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, int* OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, ID3D11Resource** OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, ID3D12Resource** OutValue) const = 0;
    virtual FNGXResult Get(const char* InName, void** OutValue) const = 0;

    virtual void Reset() = 0;
};



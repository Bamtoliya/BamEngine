#pragma once

#include "ReflectionMacro.h"

namespace Engine 
{
    ENUM()
    enum class ESamplerFilter
    {
        Point,
        Linear,
        Anisotropic
    };
    
    ENUM()
    enum class ESamplerAddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        Border,
        MirrorOnce
    };
    
    STRUCT()
    struct tagSamplerDesc
    {
        REFLECT_STRUCT()
    
        PROPERTY()
        ESamplerFilter MinFilter = ESamplerFilter::Linear;
    
        PROPERTY()
        ESamplerFilter MagFilter = ESamplerFilter::Linear;
    
        PROPERTY()
        ESamplerFilter MipFilter = ESamplerFilter::Linear;
    
        PROPERTY()
        ESamplerAddressMode AddressU = ESamplerAddressMode::Wrap;
    
        PROPERTY()
        ESamplerAddressMode AddressV = ESamplerAddressMode::Wrap;
    
        PROPERTY()
        ESamplerAddressMode AddressW = ESamplerAddressMode::Wrap;
    
        PROPERTY()
        uint32 MaxAnisotropy = 1;
    
        PROPERTY()
        vec4 BorderColor = vec4(0.f);
    };
}

template<>
struct hash<tagSamplerDesc>
{
    size_t operator()(const tagSamplerDesc& desc) const
    {
        size_t seed = 0;

        HashCombine(seed, hash<int>()((int)desc.MinFilter));
        HashCombine(seed, hash<int>()((int)desc.MagFilter));
        HashCombine(seed, hash<int>()((int)desc.MipFilter));
        HashCombine(seed, hash<int>()((int)desc.AddressU));
        HashCombine(seed, hash<int>()((int)desc.AddressV));
        HashCombine(seed, hash<int>()((int)desc.AddressW));
        HashCombine(seed, hash<uint32>()(desc.MaxAnisotropy));


        HashCombine(seed, hash<float>()(desc.BorderColor.x));
        HashCombine(seed, hash<float>()(desc.BorderColor.y));
        HashCombine(seed, hash<float>()(desc.BorderColor.z));
        HashCombine(seed, hash<float>()(desc.BorderColor.w));

        return seed;
    }
};
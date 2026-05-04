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
    
        PROPERTY(EDITABLE)
        ESamplerFilter MinFilter = ESamplerFilter::Linear;
    
        PROPERTY(EDITABLE)
        ESamplerFilter MagFilter = ESamplerFilter::Linear;
    
        PROPERTY(EDITABLE)
        ESamplerFilter MipFilter = ESamplerFilter::Linear;
    
        PROPERTY(EDITABLE)
        ESamplerAddressMode AddressU = ESamplerAddressMode::Wrap;
    
        PROPERTY(EDITABLE)
        ESamplerAddressMode AddressV = ESamplerAddressMode::Wrap;
    
        PROPERTY(EDITABLE)
        ESamplerAddressMode AddressW = ESamplerAddressMode::Wrap;
    
        PROPERTY(EDITABLE)
        uint32 MaxAnisotropy = 1;
    
        PROPERTY(EDITABLE, COLOR)
        vec4 BorderColor = vec4(0.f);

        bool operator==(const tagSamplerDesc& other) const
        {
            return MinFilter == other.MinFilter
                && MagFilter == other.MagFilter
                && MipFilter == other.MipFilter
                && AddressU == other.AddressU
                && AddressV == other.AddressV
                && AddressW == other.AddressW
                && MaxAnisotropy == other.MaxAnisotropy
                && BorderColor.x == other.BorderColor.x
                && BorderColor.y == other.BorderColor.y
                && BorderColor.z == other.BorderColor.z
                && BorderColor.w == other.BorderColor.w;
        }
    };
}

template<>
struct hash<Engine::tagSamplerDesc>
{
    size_t operator()(const Engine::tagSamplerDesc& desc) const
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
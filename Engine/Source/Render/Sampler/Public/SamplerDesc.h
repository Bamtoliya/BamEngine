#pragma once
#include <cstddef>
#include <functional>

#include "Functions.h"
#include "Reflection/ReflectionMacro.h"

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
    struct SamplerDesc
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

        bool operator==(const SamplerDesc& other) const
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
struct std::hash<Engine::SamplerDesc>
{
    std::size_t operator()(const Engine::SamplerDesc& desc) const
    {
        std::size_t seed = 0;

        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.MinFilter)));
        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.MagFilter)));
        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.MipFilter)));
        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.AddressU)));
        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.AddressV)));
        ::HashCombine(seed, std::hash<int>{}(static_cast<int>(desc.AddressW)));
        ::HashCombine(seed, std::hash<Engine::uint32>{}(desc.MaxAnisotropy));
        ::HashCombine(seed, std::hash<float>{}(desc.BorderColor.x));
        ::HashCombine(seed, std::hash<float>{}(desc.BorderColor.y));
        ::HashCombine(seed, std::hash<float>{}(desc.BorderColor.z));
        ::HashCombine(seed, std::hash<float>{}(desc.BorderColor.w));

        return seed;
    }
};
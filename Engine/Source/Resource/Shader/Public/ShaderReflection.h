#pragma once

#include "Engine_Includes.h"

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain,
	Unknown,
};

enum class EShaderResourceKind : uint8
{
    Unknown = 0,
    UniformBuffer,
    StorageBuffer,
    SampledImage,
    StorageImage,
    SeparateImage,
    SeparateSampler,
    SubpassInput,
    PushConstant,
    StageInput,
    StageOutput,
};

struct ShaderEntryPointInfo
{
    string Name = {};
    EShaderType ShaderType = EShaderType::Unknown;
};

struct ShaderResourceBindingInfo
{
    string Name = {};
    EShaderResourceKind Kind = EShaderResourceKind::Unknown;

    uint32 Set = 0;
    uint32 Binding = 0;
    uint32 Location = 0;

    uint32 ArraySize = 1;
    uint32 StructSize = 0;

    bool bWritable = false;
};

struct ShaderInterfaceVariableInfo
{
    string Name = {};
    uint32 Location = 0;

    uint32 VectorSize = 1;
    uint32 Columns = 1;
    uint32 BitWidth = 32;
};

struct ShaderReflectionInfo
{
    vector<ShaderEntryPointInfo> EntryPoints = {};

    EShaderType DefaultShaderType = EShaderType::Unknown;
    string DefaultEntryPoint = {};

    uint32 NumSamplers = 0;
    uint32 NumStorageTextures = 0;
    uint32 NumStorageBuffers = 0;
    uint32 NumUniformBuffers = 0;

    uint32 PushConstantSize = 0;

    uint32 LocalSizeX = 0;
    uint32 LocalSizeY = 0;
    uint32 LocalSizeZ = 0;

    vector<ShaderResourceBindingInfo> Resources = {};
    vector<ShaderInterfaceVariableInfo> StageInputs = {};
    vector<ShaderInterfaceVariableInfo> StageOutputs = {};
};

ENGINE_API ShaderReflectionInfo ReflectSPIRV(const vector<uint8>& bytecode);
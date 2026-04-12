#pragma once

#include "ShaderReflection.h"
#include <spirv_cross/spirv_cross_c.h>

static EShaderType ToShaderType(SpvExecutionModel model)
{
    switch (model)
    {
    case SpvExecutionModelVertex: return EShaderType::Vertex;
    case SpvExecutionModelFragment: return EShaderType::Pixel;
    case SpvExecutionModelGLCompute: return EShaderType::Compute;
    case SpvExecutionModelGeometry: return EShaderType::Geometry;
    case SpvExecutionModelTessellationControl: return EShaderType::Hull;
    case SpvExecutionModelTessellationEvaluation: return EShaderType::Domain;
    default: return EShaderType::Unknown;
    }
}

static EShaderResourceKind ToResourceKind(spvc_resource_type type)
{
    switch (type)
    {
    case SPVC_RESOURCE_TYPE_UNIFORM_BUFFER: return EShaderResourceKind::UniformBuffer;
    case SPVC_RESOURCE_TYPE_STORAGE_BUFFER: return EShaderResourceKind::StorageBuffer;
    case SPVC_RESOURCE_TYPE_SAMPLED_IMAGE: return EShaderResourceKind::SampledImage;
    case SPVC_RESOURCE_TYPE_STORAGE_IMAGE: return EShaderResourceKind::StorageImage;
    case SPVC_RESOURCE_TYPE_SEPARATE_IMAGE: return EShaderResourceKind::SeparateImage;
    case SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS: return EShaderResourceKind::SeparateSampler;
    case SPVC_RESOURCE_TYPE_SUBPASS_INPUT: return EShaderResourceKind::SubpassInput;
    case SPVC_RESOURCE_TYPE_PUSH_CONSTANT: return EShaderResourceKind::PushConstant;
    case SPVC_RESOURCE_TYPE_STAGE_INPUT: return EShaderResourceKind::StageInput;
    case SPVC_RESOURCE_TYPE_STAGE_OUTPUT: return EShaderResourceKind::StageOutput;
    default: return EShaderResourceKind::Unknown;
    }
}

static uint32 GetArraySize(spvc_compiler compiler, spvc_type_id typeId)
{
    spvc_type type = spvc_compiler_get_type_handle(compiler, typeId);
    unsigned dims = spvc_type_get_num_array_dimensions(type);
    if (dims == 0)
        return 1;

    if (spvc_type_array_dimension_is_literal(type, 0))
        return static_cast<uint32>(spvc_type_get_array_dimension(type, 0));

    return 0;
}

static uint32 GetStructSize(spvc_compiler compiler, spvc_type_id typeId)
{
    spvc_type type = spvc_compiler_get_type_handle(compiler, typeId);
    size_t size = 0;
    if (spvc_compiler_get_declared_struct_size(compiler, type, &size) == SPVC_SUCCESS)
        return static_cast<uint32>(size);
    return 0;
}

static void AppendResourceList(
    ShaderReflectionInfo& outInfo,
    spvc_compiler compiler,
    spvc_resources resources,
    spvc_resource_type resourceType,
    uint32* counter = nullptr)
{
    const spvc_reflected_resource* resourceList = nullptr;
    size_t resourceCount = 0;

    if (spvc_resources_get_resource_list_for_type(resources, resourceType, &resourceList, &resourceCount) != SPVC_SUCCESS)
        return;

    if (counter)
        *counter = static_cast<uint32>(resourceCount);

    for (size_t i = 0; i < resourceCount; ++i)
    {
        const spvc_reflected_resource& resource = resourceList[i];

        ShaderResourceBindingInfo item;
        item.Name = resource.name ? resource.name : "";
        item.Kind = ToResourceKind(resourceType);

        if (spvc_compiler_has_decoration(compiler, resource.id, SpvDecorationDescriptorSet))
            item.Set = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationDescriptorSet);

        if (spvc_compiler_has_decoration(compiler, resource.id, SpvDecorationBinding))
            item.Binding = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationBinding);

        if (spvc_compiler_has_decoration(compiler, resource.id, SpvDecorationLocation))
            item.Location = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationLocation);

        item.ArraySize = GetArraySize(compiler, resource.type_id);

        if (resourceType == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER ||
            resourceType == SPVC_RESOURCE_TYPE_STORAGE_BUFFER ||
            resourceType == SPVC_RESOURCE_TYPE_PUSH_CONSTANT)
        {
            item.StructSize = GetStructSize(compiler, resource.base_type_id);
        }

        spvc_type type = spvc_compiler_get_type_handle(compiler, resource.type_id);
        item.bWritable = spvc_type_get_image_is_storage(type) ? true : false;

        outInfo.Resources.push_back(std::move(item));
    }
}

static void AppendStageInterfaceList(
    vector<ShaderInterfaceVariableInfo>& outList,
    spvc_compiler compiler,
    spvc_resources resources,
    spvc_resource_type resourceType)
{
    const spvc_reflected_resource* resourceList = nullptr;
    size_t resourceCount = 0;

    if (spvc_resources_get_resource_list_for_type(resources, resourceType, &resourceList, &resourceCount) != SPVC_SUCCESS)
        return;

    for (size_t i = 0; i < resourceCount; ++i)
    {
        const spvc_reflected_resource& resource = resourceList[i];
        spvc_type type = spvc_compiler_get_type_handle(compiler, resource.type_id);

        ShaderInterfaceVariableInfo item;
        item.Name = resource.name ? resource.name : "";

        if (spvc_compiler_has_decoration(compiler, resource.id, SpvDecorationLocation))
            item.Location = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationLocation);

        item.VectorSize = spvc_type_get_vector_size(type);
        item.Columns = spvc_type_get_columns(type);
        item.BitWidth = spvc_type_get_bit_width(type);

        outList.push_back(std::move(item));
    }
}

ShaderReflectionInfo ReflectSPIRV(const vector<uint8>& bytecode)
{
    ShaderReflectionInfo info = {};

    if (bytecode.empty() || (bytecode.size() % sizeof(uint32)) != 0)
        return info;

    const uint32_t* spirvData = reinterpret_cast<const uint32_t*>(bytecode.data());
    size_t wordCount = bytecode.size() / sizeof(uint32_t);

    spvc_context context = nullptr;
    spvc_parsed_ir ir = nullptr;
    spvc_compiler compiler = nullptr;
    spvc_resources resources = nullptr;

    if (spvc_context_create(&context) != SPVC_SUCCESS)
        return info;

    if (spvc_context_parse_spirv(context, spirvData, wordCount, &ir) != SPVC_SUCCESS)
    {
        spvc_context_destroy(context);
        return info;
    }

    if (spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS)
    {
        spvc_context_destroy(context);
        return info;
    }

    const spvc_entry_point* entryPoints = nullptr;
    size_t numEntryPoints = 0;
    if (spvc_compiler_get_entry_points(compiler, &entryPoints, &numEntryPoints) == SPVC_SUCCESS)
    {
        for (size_t i = 0; i < numEntryPoints; ++i)
        {
            ShaderEntryPointInfo ep;
            ep.Name = entryPoints[i].name ? entryPoints[i].name : "";
            ep.ShaderType = ToShaderType(entryPoints[i].execution_model);
            info.EntryPoints.push_back(ep);
        }

        if (!info.EntryPoints.empty())
        {
            info.DefaultEntryPoint = info.EntryPoints[0].Name;
            info.DefaultShaderType = info.EntryPoints[0].ShaderType;
            spvc_compiler_set_entry_point(compiler, info.DefaultEntryPoint.c_str(), entryPoints[0].execution_model);
        }
    }

    if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS)
    {
        spvc_context_destroy(context);
        return info;
    }

    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &info.NumSamplers);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &info.NumStorageTextures);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &info.NumStorageBuffers);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &info.NumUniformBuffers);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_SUBPASS_INPUT);
    AppendResourceList(info, compiler, resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT);

    AppendStageInterfaceList(info.StageInputs, compiler, resources, SPVC_RESOURCE_TYPE_STAGE_INPUT);
    AppendStageInterfaceList(info.StageOutputs, compiler, resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT);

    for (const ShaderResourceBindingInfo& resource : info.Resources)
    {
        if (resource.Kind == EShaderResourceKind::PushConstant)
        {
            info.PushConstantSize = resource.StructSize;
            break;
        }
    }

    if (info.DefaultShaderType == EShaderType::Compute)
    {
        info.LocalSizeX = spvc_compiler_get_execution_mode_argument(compiler, SpvExecutionModeLocalSize);
        info.LocalSizeY = spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 1);
        info.LocalSizeZ = spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 2);
    }

    spvc_context_destroy(context);
    return info;
}
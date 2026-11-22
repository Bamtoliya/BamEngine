#include "DirectX12AccelerationStructure.h"
#include "DirectX12RHI.h"
#include "DirectX12Buffer.h"
#include "Logger.h"

namespace
{
	using namespace Engine;

	constexpr uint64 AlignAccelerationStructureSize(uint64 size)
	{
		constexpr uint64 alignment = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;
		return (std::max<uint64>(size, alignment) + alignment - 1) & ~(alignment - 1);
	}

	// 버퍼는 항상 COMMON으로 생성된다(다른 초기 상태는 무시되고 경고 #1328이 난다).
	// 스크래치는 빌드 시 UNORDERED_ACCESS로 암시적 승격되고, 제출이 끝나면 다시 COMMON으로 감쇠한다.
	// 결과 버퍼의 RAYTRACING_ACCELERATION_STRUCTURE 상태는 예외로 생성 시 지정해야 한다.
	constexpr D3D12_RESOURCE_STATES ScratchInitialState = D3D12_RESOURCE_STATE_COMMON;

	ComPtr<ID3D12Resource> CreateRaytracingBuffer(ID3D12Device* device, uint64 size, D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState)
	{
		const CD3DX12_HEAP_PROPERTIES heapProps(heapType);
		const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
		ComPtr<ID3D12Resource> resource;
		if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&resource))))
			return nullptr;
		return resource;
	}

	// 가속 구조 빌드 입력은 NON_PIXEL_SHADER_RESOURCE 상태여야 한다.
	// - 이전 제출에서 쓴 버퍼는 COMMON으로 감쇠해 있으므로 암시적 승격에 맡기고 배리어를 기록하지 않는다.
	// - 이번 프레임에 업로드한 버퍼는 기존 읽기 상태(VERTEX_AND_CONSTANT_BUFFER, INDEX_BUFFER)에 빌드 입력 상태를 합친다.
	void TransitionToBuildInput(DirectX12RHI& rhi, ID3D12GraphicsCommandList* commandList, DirectX12Buffer& buffer)
	{
		if (!rhi.IsBufferStateRecordedThisFrame(&buffer))
			return;

		constexpr D3D12_RESOURCE_STATES buildInput = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		const D3D12_RESOURCE_STATES writeStates = D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
			D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_COPY_DEST |
			D3D12_RESOURCE_STATE_RESOLVE_DEST;

		const D3D12_RESOURCE_STATES before = buffer.GetCurrentState();
		if ((before & buildInput) != 0)
			return;

		D3D12_RESOURCE_STATES after = buildInput;
		if (before != D3D12_RESOURCE_STATE_COMMON && (before & writeStates) == 0)
			after |= before;
		else if (buffer.GetBufferType() == ERHIBufferType::Vertex)
			after |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		else if (buffer.GetBufferType() == ERHIBufferType::Index)
			after |= D3D12_RESOURCE_STATE_INDEX_BUFFER;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = static_cast<ID3D12Resource*>(buffer.GetNativeHandle());
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		commandList->ResourceBarrier(1, &barrier);
		buffer.SetCurrentState(after);
	}

	bool CanUseRaytracing(DirectX12RHI* rhi)
	{
		return rhi && rhi->GetDevice5() && rhi->GetCommandList4() && rhi->IsFrameRecording() &&
			rhi->GetCapabilities().SupportsInlineRaytracing;
	}
}

void DirectX12AccelerationStructure::TransitionToShaderRead(DirectX12RHI& rhi, ID3D12GraphicsCommandList* commandList, DirectX12Buffer& buffer)
{
	TransitionToBuildInput(rhi, commandList, buffer);
}

DirectX12AccelerationStructure* DirectX12AccelerationStructure::CreateBottomLevel(DirectX12RHI* rhi, const RHIRaytracingGeometryDesc& desc)
{
	if (!CanUseRaytracing(rhi))
		return nullptr;

	auto* position = static_cast<DirectX12Buffer*>(desc.positionBuffer);
	auto* index = static_cast<DirectX12Buffer*>(desc.indexBuffer);
	if (!position || !position->GetNativeHandle() || desc.vertexCount == 0 || desc.vertexStride < sizeof(float) * 3)
		return nullptr;
	if (static_cast<uint64>(desc.vertexCount - 1) * desc.vertexStride + sizeof(float) * 3 > position->GetSize())
		return nullptr;

	if (index)
	{
		if (!index->GetNativeHandle() || (desc.indexStride != 2 && desc.indexStride != 4) ||
			desc.indexCount == 0 || desc.indexCount % 3 != 0 ||
			static_cast<uint64>(desc.indexCount) * desc.indexStride > index->GetSize())
			return nullptr;
	}
	else if (desc.vertexCount % 3 != 0)
	{
		return nullptr;
	}

	ID3D12Device5* device = rhi->GetDevice5();
	ID3D12GraphicsCommandList4* commandList = rhi->GetCommandList4();

	D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
	geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometry.Triangles.VertexCount = desc.vertexCount;
	geometry.Triangles.VertexBuffer.StartAddress = static_cast<ID3D12Resource*>(position->GetNativeHandle())->GetGPUVirtualAddress();
	geometry.Triangles.VertexBuffer.StrideInBytes = desc.vertexStride;
	if (index)
	{
		geometry.Triangles.IndexFormat = desc.indexStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		geometry.Triangles.IndexCount = desc.indexCount;
		geometry.Triangles.IndexBuffer = static_cast<ID3D12Resource*>(index->GetNativeHandle())->GetGPUVirtualAddress();
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = 1;
	inputs.pGeometryDescs = &geometry;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
	if (prebuild.ResultDataMaxSizeInBytes == 0)
		return nullptr;

	const uint64 resultSize = AlignAccelerationStructureSize(prebuild.ResultDataMaxSizeInBytes);
	ComPtr<ID3D12Resource> result = CreateRaytracingBuffer(device, resultSize, D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	ComPtr<ID3D12Resource> scratch = CreateRaytracingBuffer(device, AlignAccelerationStructureSize(prebuild.ScratchDataSizeInBytes),
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, ScratchInitialState);
	if (!result || !scratch)
	{
		ENGINE_LOG_ERROR("Failed to allocate BLAS buffers ({} bytes result).", resultSize);
		return nullptr;
	}

	TransitionToBuildInput(*rhi, commandList, *position);
	if (index)
		TransitionToBuildInput(*rhi, commandList, *index);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
	build.Inputs = inputs;
	build.DestAccelerationStructureData = result->GetGPUVirtualAddress();
	build.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
	commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

	// 스크래치는 이번 프레임 GPU 작업이 끝나면 해제된다.
	rhi->DeferRelease(scratch.Get());

	auto* instance = new DirectX12AccelerationStructure(rhi, ERHIAccelerationStructureType::BottomLevel);
	instance->m_Result = result;
	instance->m_ResultSizeInBytes = resultSize;
	return instance;
}

EResult DirectX12AccelerationStructure::EnsureInstanceCapacity(DirectX12RHI* rhi, uint32 frameIndex, uint32 instanceCount)
{
	if (m_InstanceBuffers[frameIndex] && instanceCount <= m_InstanceCapacity[frameIndex])
		return EResult::Success;

	const uint32 capacity = std::max({ instanceCount, m_InstanceCapacity[frameIndex] * 2, 64u });
	ComPtr<ID3D12Resource> buffer = CreateRaytracingBuffer(rhi->GetDevice5(),
		static_cast<uint64>(capacity) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
		D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
	if (!buffer)
		return EResult::OutOfMemory;

	void* mapped = nullptr;
	const D3D12_RANGE readRange = { 0, 0 };
	if (FAILED(buffer->Map(0, &readRange, &mapped)))
		return EResult::Fail;

	if (m_InstanceBuffers[frameIndex])
	{
		m_InstanceBuffers[frameIndex]->Unmap(0, nullptr);
		rhi->DeferRelease(m_InstanceBuffers[frameIndex].Get());
	}

	m_InstanceBuffers[frameIndex] = buffer;
	m_InstanceMapped[frameIndex] = mapped;
	m_InstanceCapacity[frameIndex] = capacity;
	return EResult::Success;
}

EResult DirectX12AccelerationStructure::BuildTopLevel(DirectX12RHI* rhi, RHIAccelerationStructure*& topLevel,
	const RHIRaytracingInstanceDesc* instances, uint32 instanceCount)
{
	if (!CanUseRaytracing(rhi))
		return EResult::NotImplemented;
	if (instanceCount > 0 && !instances)
		return EResult::InvalidArgument;
	if (topLevel && topLevel->GetType() != ERHIAccelerationStructureType::TopLevel)
		return EResult::InvalidArgument;

	auto* tlas = static_cast<DirectX12AccelerationStructure*>(topLevel);
	if (!tlas)
	{
		tlas = new DirectX12AccelerationStructure(rhi, ERHIAccelerationStructureType::TopLevel);
		topLevel = tlas;
	}

	const uint32 frameIndex = rhi->GetFrameResourceIndex();
	if (frameIndex >= MAX_SWAPCHAIN_BUFFERS)
		return EResult::Fail;
	if (const EResult result = tlas->EnsureInstanceCapacity(rhi, frameIndex, instanceCount); IsFailure(result))
		return result;

	// 인스턴스 기술자 작성. glm 열 우선 행렬을 D3D12의 3x4 행 우선 변환으로 옮긴다.
	auto* descs = static_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(tlas->m_InstanceMapped[frameIndex]);
	uint32 validCount = 0;
	for (uint32 i = 0; i < instanceCount; ++i)
	{
		const RHIRaytracingInstanceDesc& source = instances[i];
		auto* bottomLevel = static_cast<DirectX12AccelerationStructure*>(source.bottomLevel);
		if (!bottomLevel || bottomLevel->GetType() != ERHIAccelerationStructureType::BottomLevel || !bottomLevel->m_Result)
			continue;

		D3D12_RAYTRACING_INSTANCE_DESC desc = {};
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 4; ++column)
				desc.Transform[row][column] = source.worldMatrix[column][row];
		}
		desc.InstanceID = source.instanceID & 0xFFFFFFu;
		desc.InstanceMask = source.instanceMask;
		desc.InstanceContributionToHitGroupIndex = 0;
		// 그림자 차폐는 면 방향과 무관해야 하므로 삼각형 컬링을 끈다(음수 배율·한쪽 면 메시 포함).
		desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
		if (source.forceNonOpaque)
			desc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
		desc.AccelerationStructure = bottomLevel->GetGPUAddress();
		descs[validCount++] = desc;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = validCount;
	inputs.InstanceDescs = validCount > 0 ? tlas->m_InstanceBuffers[frameIndex]->GetGPUVirtualAddress() : 0;

	ID3D12Device5* device = rhi->GetDevice5();
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

	const uint64 resultSize = AlignAccelerationStructureSize(prebuild.ResultDataMaxSizeInBytes);
	if (!tlas->m_Result || tlas->m_ResultSizeInBytes < resultSize)
	{
		ComPtr<ID3D12Resource> result = CreateRaytracingBuffer(device, resultSize, D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
		if (!result)
			return EResult::OutOfMemory;
		if (tlas->m_Result)
			rhi->DeferRelease(tlas->m_Result.Get());
		tlas->m_Result = result;
		tlas->m_ResultSizeInBytes = resultSize;
	}

	const uint64 scratchSize = AlignAccelerationStructureSize(prebuild.ScratchDataSizeInBytes);
	if (!tlas->m_Scratch || tlas->m_ScratchSizeInBytes < scratchSize)
	{
		ComPtr<ID3D12Resource> scratch = CreateRaytracingBuffer(device, scratchSize, D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, ScratchInitialState);
		if (!scratch)
			return EResult::OutOfMemory;
		if (tlas->m_Scratch)
			rhi->DeferRelease(tlas->m_Scratch.Get());
		tlas->m_Scratch = scratch;
		tlas->m_ScratchSizeInBytes = scratchSize;
	}

	ID3D12GraphicsCommandList4* commandList = rhi->GetCommandList4();

	// 앞서 기록한 BLAS 빌드와 이전 TLAS 사용이 끝난 뒤 빌드하도록 전역 UAV 배리어를 둔다.
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = nullptr;
	commandList->ResourceBarrier(1, &barrier);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
	build.Inputs = inputs;
	build.DestAccelerationStructureData = tlas->m_Result->GetGPUVirtualAddress();
	build.ScratchAccelerationStructureData = tlas->m_Scratch->GetGPUVirtualAddress();
	commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

	// 이후 RayQuery가 완성된 TLAS를 읽도록 한다.
	barrier.UAV.pResource = tlas->m_Result.Get();
	commandList->ResourceBarrier(1, &barrier);
	return EResult::Success;
}

void DirectX12AccelerationStructure::Free()
{
	auto* rhi = static_cast<DirectX12RHI*>(m_RHI);
	for (uint32 i = 0; i < MAX_SWAPCHAIN_BUFFERS; ++i)
	{
		if (!m_InstanceBuffers[i])
			continue;
		m_InstanceBuffers[i]->Unmap(0, nullptr);
		if (rhi)
			rhi->DeferRelease(m_InstanceBuffers[i].Get());
		m_InstanceBuffers[i].Reset();
		m_InstanceMapped[i] = nullptr;
	}

	if (rhi)
	{
		rhi->DeferRelease(m_Scratch.Get());
		rhi->DeferRelease(m_Result.Get());
	}
	m_Scratch.Reset();
	m_Result.Reset();
	__super::Free();
}

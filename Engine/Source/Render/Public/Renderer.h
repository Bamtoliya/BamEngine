#pragma once

#include "Base.h"
#include "Structs.h"
#include "RHIs.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "FrustumCuller.h"
#include "RenderCommands.h"
#include "CameraBuffer.h"
#include "SkyAtmosphereComponent.h"

struct RHIPipelineDesc;

BEGIN(Engine)
enum class ERHIType
{
	Unknown,
	SDLGPU,
	DirectX12,		// LH, Z:[0,1], Y-UP Row-Major (adjust needed)
	Vulkan,			// LH, Z:[0,1], Y-DOWN Column-Major
	Metal,			// LH, Z:[0,1], Y-UP Column-Major
	WebGPU
};

struct RendererDesc
{
	ERHIType rhiType = ERHIType::Unknown;
	RHIDesc* rhiDesc = {};
};

struct ViewportCameraInfo
{
	CameraBuffer cameraBuffer;
	bool hasCamera = false;
	class RenderPass* renderPass = { nullptr };
	class Scene* uiScene = nullptr;
	entt::entity uiCanvas = entt::null;
	bool filterUICanvas = false;
	bool acceptsSceneSubmissions = true;
	// LOD를 고르는 뷰(Geometry 패스 ID)와 그 카메라. 같은 뷰의 그림자·포워드 패스가 같은 LOD를 쓰게 한다.
	RenderPassID viewID = INVALID_PASS_ID;
	CameraBuffer lodCamera = {};
	bool hasLODCamera = false;
};

class ENGINE_API Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)
	using DESC = RendererDesc;
	friend class Runtime;
	using RenderDelegate = MulticastDelegate<f32>;
	using CustomRenderCommand = function<EResult(f32, RenderPass*)>;
#pragma region Constructor&Destructor
private:
	Renderer() {}
	virtual ~Renderer() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Render
private:
	EResult BeginFrame();
	EResult EndFrame();
	EResult Render(f32 dt);
private: //Entity Render
	EResult RenderMeshes(const vector<StaticDrawCommand>& commands, RenderPass* renderPass, const Frustum* frustum = nullptr);
	EResult RenderSkinnedMeshes(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderSprite(f32 dt, vector<SpriteDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderDynamicGeometry(const vector<DynamicGeometryDrawCommand>& commands, RenderPass* renderPass);
	EResult RenderUIImages(const vector<UIImageDrawCommand>& commands, RenderPass* renderPass, uint32 width, uint32 height);
	void ClearFrameQueues();
#pragma endregion

#pragma region Queue Management
public:
	void SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ForgetRenderPass(RenderPassID passID);
	void ClearAllRenderQueues();
public: //Entity Submit
	void SubmitStatic(const StaticDrawCommand& command, RenderPassID passID);
	void SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID);
	void SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID);
	void SubmitDynamicGeometry(DynamicGeometryDrawCommand command, RenderPassID passID);
	void SubmitUIImage(const UIImageDrawCommand& command, RenderPassID passID);

#pragma endregion

#pragma region Viewport Camera Management
public:
	// viewID를 생략하면 Geometry 패스는 자기 ID를 뷰로 쓰고, 다른 패스는 자기 카메라로 이력 없이 LOD를 고른다.
	void RegisterViewportCamera(const CameraBuffer& cameraBuffer, RenderPass* renderPass, bool acceptsSceneSubmissions = true,
		RenderPassID viewID = INVALID_PASS_ID);
	// 자체 카메라가 없는 그림자 패스를 뷰의 카메라와 함께 등록한다. 카메라는 LOD 선택에만 쓴다.
	void RegisterShadowPassForView(RenderPass* renderPass, const CameraBuffer& viewCamera, RenderPassID viewID);
	void RegisterViewportCamera(RenderPass* renderPass);
	void RegisterUICanvas(class Scene* scene, entt::entity canvas, RenderPass* renderPass);
	void UnregisterViewportCamera(RenderPassID passID);
	vector<ViewportCameraInfo>& GetActiveViewportCameras() { return m_ViewportCameras; }
	vector<ViewportCameraInfo> CollectGeometryViewportCameras() const;
#pragma endregion

#pragma region Binding
private:
	EResult PreparePassPipelineDesc(RenderPass* renderPass, RHIPipelineDesc& desc);
	EResult BindMeshPipeline(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc, class RHIPipeline** inOutLastPipeline = nullptr);
	EResult PrepareMeshPipelineDesc(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc);
public:
	EResult BindMeshPipeline(class Mesh*, class MaterialInterface*, class RenderPass*);
	EResult ValidateMeshMaterial(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass);
#pragma endregion

#pragma region Getter
public:
	class RHI* GetRHI() const { return m_RHI; }
	RenderDelegate& GetRenderPassDelegate(RenderPassID passID)
	{
		return m_RenderPassDelegates[passID];
	}
	RenderDelegate& GetPreRenderDelegate() { return m_PreRenderDelegate; }
	ERHIType GetRHIType() const { return m_RHIType; }
#pragma endregion

#pragma region Default Shaders
public:
	// 정적 메시 섀도 패스의 엔진 기본 셰이더. 재질의 그림자 셰이더가 기본값인지 판정할 때 사용한다.
	class Shader* GetDefaultShadowDepthVertexShader();
	class Shader* GetDefaultShadowDepthPixelShader();
private:
	void EnsureDefaultShaders();
	// 섀도 패스에서 알파 기준값을 적용할 수 있는 재질인지. 알파 식을 아는 엔진 기본 GBufferPS를 쓰는 Masked 재질만 지원한다.
	bool UsesMaskedShadow(Mesh* mesh, MaterialInterface* material);
	class Shader* ResolveVertexShader(Mesh* mesh, MaterialInterface* material, ERenderPassType passType);
#pragma endregion


#pragma region Variable
private:
	class RHI* m_RHI = { nullptr };
	ERHIType m_RHIType = ERHIType::Unknown;
	RenderDelegate m_PreRenderDelegate;
	map<RenderPassID, RenderDelegate> m_RenderPassDelegates;
	map<RenderPassID, vector<CustomRenderCommand>> m_CustomRenderQueues;
private: //ECS Based Render Queue
	map<RenderPassID, vector<StaticDrawCommand>> m_StaticDrawCommands;
	map<RenderPassID, vector<SkinnedDrawCommand>> m_SkinnedDrawCommands;
	map<RenderPassID, vector<SpriteDrawCommand>> m_SpriteDrawCommands;
	map<RenderPassID, vector<DynamicGeometryDrawCommand>> m_DynamicGeometryDrawCommands;
	map<RenderPassID, vector<UIImageDrawCommand>> m_UIImageDrawCommands;
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
private:
	struct ViewportJob
	{
		CameraBuffer cameraBuffer = {};
		bool hasCamera = false;
	};

	vector<ViewportCameraInfo> m_ViewportCameras;
	unordered_map<RenderPassID, vector<ViewportJob>> m_ViewportJobsPerPass;
private:
	ResourceHandle<Shader> m_DefaultGBufferVS;
	ResourceHandle<Shader> m_DefaultGBufferSkinningVS;
	ResourceHandle<Shader> m_DefaultGBufferPS;
	ResourceHandle<Shader> m_DefaultVS;
	ResourceHandle<Shader> m_DefaultShadowDepthVS;
	ResourceHandle<Shader> m_DefaultShadowDepthSkinningVS;
	ResourceHandle<Shader> m_DefaultShadowDepthPS;
	ResourceHandle<Shader> m_MaskedShadowDepthVS;
	ResourceHandle<Shader> m_MaskedShadowDepthSkinningVS;
	ResourceHandle<Shader> m_MaskedShadowDepthPS;
#pragma endregion
};
END

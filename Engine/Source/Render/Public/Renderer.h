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
	// LOD와 방향광 캐스케이드를 정하는 뷰(Geometry 패스 ID)와 그 카메라. 같은 뷰의 그림자·포워드 패스가 같은 값을 쓰게 한다.
	RenderPassID viewID = INVALID_PASS_ID;
	CameraBuffer lodCamera = {};
	bool hasLODCamera = false;
};

class MotionHistory;

// 뷰 하나의 움직인 물체를 모션 벡터 타깃에 그리는 데 필요한 값
struct MotionVectorInfo
{
	RenderPassID geometryPass = INVALID_PASS_ID;
	mat4 jitteredViewProjection{ 1.0f }; // G-Buffer를 그린 행렬
	mat4 viewProjection{ 1.0f };         // 지터를 뺀 이번 프레임
	mat4 previousViewProjection{ 1.0f }; // 지터를 뺀 이전 프레임
	class Shader* staticVertexShader = nullptr;
	class Shader* skinnedVertexShader = nullptr;
	class Shader* terrainVertexShader = nullptr; // 없으면 움직인 지형은 카메라 이동만 반영한다.
	class Shader* pixelShader = nullptr;
	class RHITexture* position = nullptr; // G-Buffer 위치
	bool draw = false; // false면 다음 프레임을 위해 변환만 기록한다.
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
public:
	// 뷰의 Geometry 패스에 제출된 물체를 기록하고, 직전 프레임에서 움직인 물체만 renderPass에 그린다. 패스의 커스텀 명령 안에서 호출한다.
	EResult RenderMotionVectors(const MotionVectorInfo& info, MotionHistory& history, RenderPass* renderPass);
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
	bool HasDrawCommands(RenderPassID passID) const;

#pragma endregion

#pragma region Viewport Camera Management
public:
	// viewID를 생략하면 Geometry 패스는 자기 ID를 뷰로 쓰고, 다른 패스는 자기 카메라로 이력 없이 LOD를 고른다.
	void RegisterViewportCamera(const CameraBuffer& cameraBuffer, RenderPass* renderPass, bool acceptsSceneSubmissions = true,
		RenderPassID viewID = INVALID_PASS_ID);
	// 자체 카메라가 없는 그림자 패스를 뷰의 카메라와 함께 등록한다. 카메라는 LOD 선택과 방향광 캐스케이드에 쓴다.
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
	// mirrored면 월드 행렬이 감긴 방향을 뒤집으므로 재질의 앞면 방향을 반대로 쓴다.
	EResult BindMeshPipeline(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc, class RHIPipeline** inOutLastPipeline = nullptr, bool mirrored = false, bool isTerrainGrid = true);
	EResult PrepareMeshPipelineDesc(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc, bool mirrored = false, bool isTerrainGrid = true);
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
	class Shader* GetDefaultShadowDepthSkinningVertexShader();
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
		// 자체 카메라가 없는 그림자 패스가 덮을 뷰의 카메라
		CameraBuffer viewCamera = {};
		bool hasViewCamera = false;
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

#include "PostProcessPipeline.h"
#include "PostProcessMaterialEffect.h"
#include "ResourceManager.h"
#include "Material.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "PostProcessTypes.h"
#include "BloomEffect.h"
#include <archive/JsonArchive.h>

PostProcessPipeline::~PostProcessPipeline()
{
	Free();
}

void PostProcessPipeline::Initialize(const wstring& prefix)
{
	m_Prefix = prefix;
	ResourceManager& rm = ResourceManager::Get();

	// 0. 블룸(Bloom) 등록 (톤매핑 직전에 배치)
	BloomEffect* bloomEffect = new BloomEffect();
	bloomEffect->Initialize(prefix);
	bloomEffect->SetEnabled(false);
	AddEffect(bloomEffect);

	// 1. 머티리얼 기반 톤매핑 등록
	auto toneMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/ToneMappingMaterial");
	if (!toneMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/ToneMappingMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_ToneMappingPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* toneMat = Material::Create(&desc);
		if (toneMat)
		{
			toneMat->AddFloat("Exposure", 1.0f, 0.1f, 5.0f, "TONE_MAPPING_EXPOSURE");
			toneMat->AddFloat("Gamma", 2.2f, 1.0f, 3.0f, "TONE_MAPPING_GAMMA");
			toneMatHandle = rm.AddResource<Material>(desc.Key, toneMat);
		}
	}

	if (toneMatHandle)
	{
		PostProcessMaterialEffect* toneEffect = new PostProcessMaterialEffect(L"Tone Mapping (ACES)", toneMatHandle, "POST_PROCESS_TONE_MAPPING");
		toneEffect->Initialize(prefix);
		toneEffect->SetEnabled(true);
		AddEffect(toneEffect);
	}

	// 2. 머티리얼 기반 색상 조정 등록
	auto colorMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/ColorAdjustmentsMaterial");
	if (!colorMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/ColorAdjustmentsMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_ColorAdjustmentsPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* colorMat = Material::Create(&desc);
		if (colorMat)
		{
			colorMat->AddFloat("Saturation", 1.0f, 0.0f, 2.0f, "COLOR_ADJUSTMENTS_SATURATION");
			colorMat->AddFloat("Contrast", 1.0f, 0.5f, 2.0f, "COLOR_ADJUSTMENTS_CONTRAST");
			colorMat->AddFloat("Brightness", 0.0f, -1.0f, 1.0f, "COLOR_ADJUSTMENTS_BRIGHTNESS");
			colorMatHandle = rm.AddResource<Material>(desc.Key, colorMat);
		}
	}

	if (colorMatHandle)
	{
		PostProcessMaterialEffect* colorEffect = new PostProcessMaterialEffect(L"Color Adjustments", colorMatHandle, "POST_PROCESS_COLOR_ADJUSTMENTS");
		colorEffect->Initialize(prefix);
		colorEffect->SetEnabled(false);
		AddEffect(colorEffect);
	}

	// 3. 머티리얼 기반 비네팅 등록
	auto vignetteMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/VignetteMaterial");
	if (!vignetteMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/VignetteMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_VignettePS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* vignetteMat = Material::Create(&desc);
		if (vignetteMat)
		{
			vignetteMat->AddFloat("Intensity", 0.4f, 0.0f, 1.0f, "VIGNETTE_INTENSITY");
			vignetteMat->AddFloat("Smoothness", 0.5f, 0.01f, 1.0f, "VIGNETTE_SMOOTHNESS");
			vignetteMat->AddFloat("Roundness", 1.0f, 0.1f, 2.0f, "VIGNETTE_ROUNDNESS");
			vignetteMatHandle = rm.AddResource<Material>(desc.Key, vignetteMat);
		}
	}

	if (vignetteMatHandle)
	{
		PostProcessMaterialEffect* vignetteEffect = new PostProcessMaterialEffect(L"Vignette", vignetteMatHandle, "POST_PROCESS_VIGNETTE");
		vignetteEffect->Initialize(prefix);
		vignetteEffect->SetEnabled(false);
		AddEffect(vignetteEffect);
	}

	// 4. 머티리얼 기반 색수차(Chromatic Aberration) 등록
	auto chromaticMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/ChromaticAberrationMaterial");
	if (!chromaticMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/ChromaticAberrationMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_ChromaticAberrationPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* chromaticMat = Material::Create(&desc);
		if (chromaticMat)
		{
			chromaticMat->AddFloat("Intensity", 0.005f, 0.0f, 0.05f, "CHROMATIC_ABERRATION_INTENSITY");
			chromaticMat->AddFloat("Falloff", 1.5f, 0.5f, 3.0f, "CHROMATIC_ABERRATION_FALLOFF");
			chromaticMatHandle = rm.AddResource<Material>(desc.Key, chromaticMat);
		}
	}

	if (chromaticMatHandle)
	{
		PostProcessMaterialEffect* chromaticEffect = new PostProcessMaterialEffect(L"Chromatic Aberration", chromaticMatHandle, "POST_PROCESS_CHROMATIC_ABERRATION");
		chromaticEffect->Initialize(prefix);
		chromaticEffect->SetEnabled(false);
		AddEffect(chromaticEffect);
	}

	// 5. 머티리얼 기반 필름 그레인(Film Grain) 등록
	auto grainMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/FilmGrainMaterial");
	if (!grainMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/FilmGrainMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_FilmGrainPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* grainMat = Material::Create(&desc);
		if (grainMat)
		{
			grainMat->AddFloat("Intensity", 0.05f, 0.0f, 0.5f, "FILM_GRAIN_INTENSITY");
			grainMat->AddFloat("LuminanceBias", 0.5f, 0.0f, 1.0f, "FILM_GRAIN_LUMINANCE_BIAS");
			grainMat->AddFloat("Scale", 1.0f, 0.1f, 5.0f, "FILM_GRAIN_SCALE");
			grainMatHandle = rm.AddResource<Material>(desc.Key, grainMat);
		}
	}

	if (grainMatHandle)
	{
		PostProcessMaterialEffect* grainEffect = new PostProcessMaterialEffect(L"Film Grain", grainMatHandle, "POST_PROCESS_FILM_GRAIN");
		grainEffect->Initialize(prefix);
		grainEffect->SetEnabled(false);
		AddEffect(grainEffect);
	}

	// 6. 머티리얼 기반 렌즈 왜곡(Lens Distortion) 등록
	auto lensMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/LensDistortionMaterial");
	if (!lensMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/LensDistortionMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_LensDistortionPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* lensMat = Material::Create(&desc);
		if (lensMat)
		{
			lensMat->AddFloat("Distortion", 0.0f, -1.0f, 1.0f, "LENS_DISTORTION_INTENSITY");
			lensMat->AddFloat("CubicDistortion", 0.0f, -1.0f, 1.0f, "LENS_DISTORTION_CUBIC");
			lensMat->AddFloat("Scale", 1.0f, 0.5f, 2.0f, "LENS_DISTORTION_SCALE");
			lensMatHandle = rm.AddResource<Material>(desc.Key, lensMat);
		}
	}

	if (lensMatHandle)
	{
		PostProcessMaterialEffect* lensEffect = new PostProcessMaterialEffect(L"Lens Distortion", lensMatHandle, "POST_PROCESS_LENS_DISTORTION");
		lensEffect->Initialize(prefix);
		lensEffect->SetEnabled(false);
		AddEffect(lensEffect);
	}

	// 7. 머티리얼 기반 선명화(Sharpen) 등록
	auto sharpenMatHandle = rm.GetResourceHandle<Material>(L"Resources/Material/SharpenMaterial");
	if (!sharpenMatHandle)
	{
		MaterialDesc desc;
		desc.Key = L"Resources/Material/SharpenMaterial";
		desc.VertexShaderHandle = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS");
		desc.PixelShaderHandle = rm.GetResourceHandle<Shader>(L"PostProcess_SharpenPS");
		desc.CullMode = ECullMode::None;
		desc.DepthMode = EDepthMode::None;

		Material* sharpenMat = Material::Create(&desc);
		if (sharpenMat)
		{
			sharpenMat->AddFloat("Sharpness", 0.5f, 0.0f, 2.0f, "SHARPEN_SHARPNESS");
			sharpenMatHandle = rm.AddResource<Material>(desc.Key, sharpenMat);
		}
	}

	if (sharpenMatHandle)
	{
		PostProcessMaterialEffect* sharpenEffect = new PostProcessMaterialEffect(L"Sharpen", sharpenMatHandle, "POST_PROCESS_SHARPEN");
		sharpenEffect->Initialize(prefix);
		sharpenEffect->SetEnabled(false);
		AddEffect(sharpenEffect);
	}
}

void PostProcessPipeline::PrepareResources(uint32 width, uint32 height)
{
	if (width == 0 || height == 0) return;
	m_Width = width;
	m_Height = height;

	auto& rtMgr = RenderTargetManager::Get();
	m_PPRTNames[0] = m_Prefix + L"_PP_A";
	m_PPRTNames[1] = m_Prefix + L"_PP_B";

	// 핑퐁 렌더타깃 생성
	for (int i = 0; i < 2; ++i)
	{
		RenderTargetDesc desc = {
			{ width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM,
			  ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f) },
			m_PPRTNames[i]
		};
		rtMgr.CreateRenderTarget(&desc);
	}

	// 핑퐁 렌더패스 등록 (MAX_PP_PASSES = 16)
	auto& rpMgr = RenderPassManager::Get();
	m_PassIDs.clear();
	for (uint32 pi = 0; pi < MAX_PP_PASSES; ++pi)
	{
		RenderPassID rpID = rpMgr.RegisterRenderPass(
			m_Prefix + L"_PPPass_" + std::to_wstring(pi), { m_PPRTNames[pi % 2] }, L"",
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.f, 0.f, -1.f), 130 + pi, ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::None);
		m_PassIDs.push_back(rpID);
	}
}

EResult PostProcessPipeline::Resize(uint32 width, uint32 height)
{
	if (width == 0 || height == 0)
		return EResult::InvalidArgument;

	auto& rtMgr = RenderTargetManager::Get();
	bool succeeded = true;

	for (const auto& name : m_PPRTNames)
	{
		auto* target = rtMgr.GetRenderTarget(name);

		if (!target || target->Resize(width, height) != EResult::Success)
			succeeded = false;
	}

	if (!succeeded)
		return EResult::Fail;

	m_Width = width;
	m_Height = height;
	return EResult::Success;
}

wstring PostProcessPipeline::Execute(f32 dt, const wstring& inputRT)
{
	// 활성화된 효과가 하나도 없으면 패스를 타지 않고 원본 RT를 즉시 반환 (Zero-Cost Pass-through)
	wstring currentRT = inputRT;
	uint32 slot = 0;

	for (auto* effect : m_Effects)
	{
		if (!effect || !effect->IsEnabled()) continue;
		if (slot >= m_PassIDs.size()) break;

		const wstring writeRT = m_PPRTNames[slot % 2];
		effect->SubmitPass(dt, m_PassIDs[slot], currentRT);

		currentRT = writeRT;
		++slot;
	}

	return currentRT;
}

void PostProcessPipeline::Free()
{
	for (auto* effect : m_Effects)
	{
		if (effect)
		{
			effect->Free();
			delete effect;
		}
	}
	m_Effects.clear();
	m_PassIDs.clear();
}

void PostProcessPipeline::AddEffect(IPostProcessEffect* effect)
{
	if (effect)
		m_Effects.push_back(effect);
}

#pragma region Save & Load
archive::ArchiveResult PostProcessPipeline::WriteArchive(archive::IArchive& ar) const
{
	return ar.WriteObjectArray("Effects", m_Effects.size(),
		[this](archive::IArchive& temporary, std::size_t index) -> archive::ArchiveResult
		{
			auto* effect = m_Effects[index];
			if (!effect) return {};

			// 1. 효과 식별 정보 저장 (효과명, 활성화 여부)
			const auto nameRes = temporary.Write("Name", WStrToStr(effect->GetName()));
			if (!nameRes) return nameRes;

			const auto activeRes = temporary.Write("Enabled", effect->IsEnabled());
			if (!activeRes) return activeRes;

			// 2. 효과가 보유한 모든 파라미터 직렬화 (방안 B 덕분에 단일 제네릭 루프로 처리)
			const auto& params = effect->GetParameters();
			return temporary.WriteObject("Parameters", params,
				[&params](archive::IArchive& paramArchive, const auto&) -> archive::ArchiveResult
				{
					for (const auto& [paramName, param] : params)
					{
						switch (param.type)
						{
						case EMaterialParameterType::Float:
						{
							float val = *reinterpret_cast<const float*>(param.data.data());
							auto r = paramArchive.Write(paramName, val);
							if (!r) return r;
							break;
						}
						case EMaterialParameterType::Float2:
						{
							vec2 val = *reinterpret_cast<const vec2*>(param.data.data());
							std::vector<float> arr = { val.x, val.y };
							auto r = paramArchive.WriteArray(paramName, arr);
							if (!r) return r;
							break;
						}
						case EMaterialParameterType::Float3:
						{
							vec3 val = *reinterpret_cast<const vec3*>(param.data.data());
							std::vector<float> arr = { val.x, val.y, val.z };
							auto r = paramArchive.WriteArray(paramName, arr);
							if (!r) return r;
							break;
						}
						case EMaterialParameterType::Float4:
						{
							vec4 val = *reinterpret_cast<const vec4*>(param.data.data());
							std::vector<float> arr = { val.x, val.y, val.z, val.w };
							auto r = paramArchive.WriteArray(paramName, arr);
							if (!r) return r;
							break;
						}
						case EMaterialParameterType::Int:
						{
							int val = *reinterpret_cast<const int*>(param.data.data());
							auto r = paramArchive.Write(paramName, val);
							if (!r) return r;
							break;
						}
						case EMaterialParameterType::Bool:
						{
							bool val = *reinterpret_cast<const bool*>(param.data.data());
							auto r = paramArchive.Write(paramName, val);
							if (!r) return r;
							break;
						}
						default:
							break;
						}
					}
					return {};
				});
		});
}

archive::ArchiveResult PostProcessPipeline::ReadArchive(const archive::IArchive& ar)
{
	using archive::ArchiveErrorCode;
	using archive::ArchiveResult;

	std::vector<IPostProcessEffect*> reorderedEffects;
	std::vector<std::function<void()>> pendingChanges;
	reorderedEffects.reserve(m_Effects.size());

	const auto result = ar.ReadObjectArray("Effects",
		[this, &reorderedEffects, &pendingChanges](archive::IArchive& temporary, std::size_t) -> ArchiveResult
		{
			std::string effectName;
			auto readResult = temporary.Read("Name", effectName);

			if (!readResult)
				return readResult;

			IPostProcessEffect* matched = nullptr;

			for (auto* effect : m_Effects)
			{
				if (effect && WStrToStr(effect->GetName()) == effectName)
				{
					matched = effect;
					break;
				}
			}

			// 현재 지원하지 않는 효과는 기존 호환 정책에 따라 건너뛴다.
			if (!matched)
				return {};

			if (std::find(reorderedEffects.begin(), reorderedEffects.end(), matched) != reorderedEffects.end())
				return { ArchiveErrorCode::InvalidValue, "Name" };

			reorderedEffects.push_back(matched);

			bool hasEnabled = false;
			readResult = temporary.HasField("Enabled", hasEnabled);

			if (!readResult)
				return readResult;

			if (hasEnabled)
			{
				bool enabled = false;
				readResult = temporary.Read("Enabled", enabled);

				if (!readResult)
					return readResult;

				pendingChanges.emplace_back([matched, enabled] { matched->SetEnabled(enabled); });
			}

			bool hasParameters = false;
			readResult = temporary.HasField("Parameters", hasParameters);

			if (!readResult)
				return readResult;

			if (!hasParameters)
				return {};

			bool unused = false;

			return temporary.ReadObject("Parameters", unused,
				[matched, &pendingChanges](archive::IArchive& parameters, bool&) -> ArchiveResult
				{
					for (const auto& [name, parameter] : matched->GetParameters())
					{
						bool exists = false;
						auto parameterResult = parameters.HasField(name, exists);

						if (!parameterResult)
							return parameterResult;

						if (!exists)
							continue;

						switch (parameter.type)
						{
						case EMaterialParameterType::Float:
						{
							float value = 0.0f;
							parameterResult = parameters.Read(name, value);

							if (!parameterResult)
								return parameterResult;

							if (!std::isfinite(value))
								return { ArchiveErrorCode::InvalidValue, name };

							pendingChanges.emplace_back([matched, name, value]
								{
									matched->SetFloat(name, value);
								});
							break;
						}
						case EMaterialParameterType::Float2:
						case EMaterialParameterType::Float3:
						case EMaterialParameterType::Float4:
						{
							std::vector<float> values;
							parameterResult = parameters.ReadArray(name, values);

							if (!parameterResult)
								return parameterResult;

							const std::size_t required = parameter.type == EMaterialParameterType::Float2 ? 2
								: parameter.type == EMaterialParameterType::Float3 ? 3 : 4;
							const bool finite = std::all_of(values.begin(), values.end(),
								[](float value) { return std::isfinite(value); });

							if (values.size() != required || !finite)
								return { ArchiveErrorCode::InvalidValue, name };

							pendingChanges.emplace_back([matched, name, values = std::move(values)]
								{
									if (values.size() == 2)
										matched->SetFloat2(name, vec2(values[0], values[1]));
									else if (values.size() == 3)
										matched->SetFloat3(name, vec3(values[0], values[1], values[2]));
									else
										matched->SetFloat4(name, vec4(values[0], values[1], values[2], values[3]));
								});
							break;
						}
						case EMaterialParameterType::Int:
						{
							int32 value = 0;
							parameterResult = parameters.Read(name, value);

							if (!parameterResult)
								return parameterResult;

							pendingChanges.emplace_back([matched, name, value]
								{
									matched->SetInt(name, value);
								});
							break;
						}
						case EMaterialParameterType::Bool:
						{
							bool value = false;
							parameterResult = parameters.Read(name, value);

							if (!parameterResult)
								return parameterResult;

							pendingChanges.emplace_back([matched, name, value]
								{
									matched->SetBool(name, value);
								});
							break;
						}
						default:
							return { ArchiveErrorCode::UnsupportedType, name };
						}
					}

					return {};
				});
		});

	if (!result)
		return result;

	// 파일에 없는 기존 효과는 원래 상대 순서대로 뒤에 유지한다.
	for (auto* effect : m_Effects)
	{
		if (effect && std::find(reorderedEffects.begin(), reorderedEffects.end(), effect) == reorderedEffects.end())
			reorderedEffects.push_back(effect);
	}

	for (const auto& apply : pendingChanges)
		apply();

	m_Effects.swap(reorderedEffects);
	return {};
}

bool PostProcessPipeline::SaveToFile(const wstring& filepath) const
{
	archive::JsonArchive writer;
	if (!WriteArchive(writer)) return false;
	const auto res = writer.SaveToFile(filepath);
	return res.Code == archive::ArchiveErrorCode::None;
}

bool PostProcessPipeline::LoadFromFile(const wstring& filepath)
{
	archive::JsonArchive reader;
	const auto loadRes = reader.LoadFromFile(filepath);
	if (loadRes.Code != archive::ArchiveErrorCode::None) return false;
	const auto readRes = ReadArchive(reader);
	return readRes.Code == archive::ArchiveErrorCode::None;
}

bool PostProcessPipeline::SaveToString(std::string& outJson) const
{
	archive::JsonArchive writer;
	if (!WriteArchive(writer)) return false;
	const auto res = writer.Encode(outJson);
	return res.Code == archive::ArchiveErrorCode::None;
}

bool PostProcessPipeline::LoadFromString(std::string_view inJson)
{
	archive::JsonArchive reader;
	const auto loadRes = reader.Decode(inJson);
	if (loadRes.Code != archive::ArchiveErrorCode::None) return false;
	const auto readRes = ReadArchive(reader);
	return readRes.Code == archive::ArchiveErrorCode::None;
}
#pragma endregion
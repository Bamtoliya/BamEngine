#pragma once

#include <memory>
#include <string>
#include "ShaderCompiler.h"
#include "ResourceHandle.h"
#include "Shader.h"
#include "RHIPipeline.h"
#include "RenderPass.h"

namespace Editor
{
	class GraphShaderPreview final
	{
	public:
		~GraphShaderPreview();

		GraphShaderPreview(const GraphShaderPreview&) = delete;
		GraphShaderPreview& operator=(const GraphShaderPreview&) = delete;

		static std::shared_ptr<GraphShaderPreview> Create(const Engine::ShaderCompileResult& compiled,
			Engine::ERHIType target, const std::string& source, std::string& outErrorKey);

		Engine::EResult Render();
		Engine::RHITexture* GetTexture() const;
		const std::string& GetSource() const { return m_Source; }

	private:
		GraphShaderPreview() = default;

		Engine::ResourceHandle<Engine::Shader> m_VertexShader;
		Engine::RHIShader* m_PixelShader = nullptr;
		Engine::RHIPipeline* m_Pipeline = nullptr;
		Engine::RenderPass* m_Pass = nullptr;
		std::wstring m_TargetName;
		std::string m_Source;
	};
}
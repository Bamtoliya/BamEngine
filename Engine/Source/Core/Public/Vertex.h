#pragma once

#include "Types.h"
#include "RenderTypes.h"
#include "ReflectionMacro.h"

namespace Engine
{
	enum class EVertexElementFormat
	{
		Invalid,

		/* 32-bit Signed Integers */
		Int,
		Int2,
		Int3,
		Int4,

		/* 32-bit Unsigned Integers */
		UInt,
		UInt2,
		UInt3,
		UInt4,

		/* 32-bit Floats (가장 많이 사용됨) */
		Float,
		Float2,
		Float3,
		Float4,

		/* 8-bit Signed Integers */
		Byte2,
		Byte4,

		/* 8-bit Unsigned Integers */
		UByte2,
		UByte4,

		/* 8-bit Signed Normalized (정규화: -1.0 ~ 1.0) */
		Byte2_Norm,
		Byte4_Norm,

		/* 8-bit Unsigned Normalized (정규화: 0.0 ~ 1.0, 텍스처/컬러 압축에 자주 쓰임) */
		UByte2_Norm,
		UByte4_Norm,

		/* 16-bit Signed Integers */
		Short2,
		Short4,

		/* 16-bit Unsigned Integers */
		UShort2,
		UShort4,

		/* 16-bit Signed Normalized */
		Short2_Norm,
		Short4_Norm,

		/* 16-bit Unsigned Normalized */
		UShort2_Norm,
		UShort4_Norm,

		/* 16-bit Floats (Half Precision) */
		Half2,
		Half4,
	};

	struct tagVertexElementDesc
	{
		uint32 Location = { 0 };
		EVertexElementFormat Format = EVertexElementFormat::Float3;
		uint32 Offset = { 0 };

		bool operator==(const tagVertexElementDesc& other) const
		{
			return Location == other.Location &&
				Format == other.Format &&
				Offset == other.Offset;
		}
	};

	enum class EVertexInputRate
	{
		PerVertex,
		PerInstance,
	};

	struct tagInputLayoutDesc
	{
		std::vector<tagVertexElementDesc> Elements;
		uint32 Stride = { 0 };
		EVertexInputRate InputRate = EVertexInputRate::PerVertex;
		uint32 InstanceDataStepRate = { 0 }; // PerInstance일 때, 인스턴스마다 데이터를 몇 개씩 건너뛸지 설정 (0이면 자동으로 1로 간주)

		bool operator==(const tagInputLayoutDesc& other) const
		{
			return Stride == other.Stride &&
				Elements == other.Elements;
		}
	};

#pragma region Vertex
	STRUCT()
	struct ENGINE_API VertexPosition
	{
		REFLECT_STRUCT();
		PROPERTY()
		glm::vec3 position = {};
		VertexPosition() = default;
		VertexPosition(const glm::vec3& pos)
			: position(pos)
		{
		}
		static const tagInputLayoutDesc Layout;
	};

	STRUCT()
	struct ENGINE_API VertexMaterial
	{
		REFLECT_STRUCT();
		PROPERTY()
		glm::vec3 normal = {};
		PROPERTY()
		glm::vec2 texCoord = {};
		PROPERTY()
		glm::vec3 tangent = {};
		PROPERTY()
		glm::vec3 bitangent = {};
		PROPERTY()
		glm::vec4 color = { 1.f, 1.f, 1.f, 1.f };
		VertexMaterial() = default;
		VertexMaterial(const glm::vec3& nor, const glm::vec2& uv, const glm::vec3& tan, const glm::vec3& bitan, const glm::vec4& col)
			: normal(nor), texCoord(uv), tangent(tan), bitangent(bitan), color(col) {
		}
		static const tagInputLayoutDesc Layout;
	};

	struct VertexSkinData
	{
		glm::uvec4 boneIDs;
		glm::vec4 weights;
		static const tagInputLayoutDesc Layout;
	};

	STRUCT()
		struct ENGINE_API Vertex
	{
		REFLECT_STRUCT();

		PROPERTY()
			glm::vec3 position = {};
		PROPERTY()
			glm::vec3 normal = {};
		PROPERTY()
			glm::vec2 texCoord = {};
		PROPERTY()
			glm::vec3 tangent = {};
		PROPERTY()
			glm::vec3 bitangent = {};
		PROPERTY()
			glm::vec4 color = { 1.f, 1.f, 1.f, 1.f };

		Vertex() = default;
		Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv)
			: position(pos), normal(norm), texCoord(uv)
		{
		}

		Vertex(const glm::vec3& pos, const glm::vec2& uv)
			: position(pos), texCoord(uv)
		{
		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan)
			: position(pos), normal(nor), texCoord(uv), tangent(tan)
		{

		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan,
			const glm::vec3& bitan,
			const glm::vec4& col)
			: position(pos), normal(nor), texCoord(uv), tangent(tan), bitangent(bitan), color(col)
		{
		}

		static const tagInputLayoutDesc Layout;
	};

	struct Vertex2D
	{
		glm::vec3 position = {};
		glm::vec4 color = {};
		glm::vec2 texCoord = {};

		static const tagInputLayoutDesc Layout;
	};



#ifdef _DEBUG
	struct ENGINE_API DebugVertex
	{
		vec3 position;
		vec4 color;

		static const tagInputLayoutDesc Layout;
	};
#endif
	struct ENGINE_API SceneUBO
	{
		mat4 worldMatrix;
	};
#pragma endregion
}
#pragma once

#include "CameraBuffer.h"
#include "Types.h"

namespace Engine
{
	// 뷰 하나의 반사 누적 이력. 어느 텍스처를 읽고 쓸지와 이전 카메라를 이어 간다.
	class ReflectionHistory
	{
	public:
		struct Frame
		{
			bool Valid = false;         // 이전 결과를 이어 쓸 수 있는지
			bool CameraMoved = true;    // 이전 프레임과 시점이 다른지
			uint32 ReadIndex = 1;       // 이전 프레임의 텍스처 (0 또는 1)
			uint32 WriteIndex = 0;
			uint32 FrameIndex = 0;
			mat4 PrevViewProjection{ 1.0f };
			vec3 PrevCameraPosition{ 0.0f };
		};

		// 이번 프레임을 시작한다. 씬이나 크기가 바뀌었거나 카메라가 크게 건너뛰면 이력을 버린다.
		Frame Begin(const CameraBuffer& camera, const void* scene, uint32 width, uint32 height)
		{
			Frame frame;
			frame.WriteIndex = m_NextWriteIndex;
			frame.ReadIndex = 1u - m_NextWriteIndex;
			frame.FrameIndex = m_FrameIndex;
			frame.PrevViewProjection = m_PrevViewProjection;
			frame.PrevCameraPosition = m_PrevCameraPosition;
			frame.Valid = m_HasHistory && m_Scene == scene && m_Width == width && m_Height == height &&
				glm::distance(m_PrevCameraPosition, camera.cameraPosition) < MaxCameraStep;
			frame.CameraMoved = !IsSameView(m_PrevViewProjection, camera.viewProjMatrix);

			m_PrevViewProjection = camera.viewProjMatrix;
			m_PrevCameraPosition = camera.cameraPosition;
			m_Scene = scene;
			m_Width = width;
			m_Height = height;
			m_NextWriteIndex = frame.ReadIndex;
			++m_FrameIndex;
			// 모든 패스가 끝나야 다음 프레임이 이어 쓸 수 있다.
			m_HasHistory = false;
			return frame;
		}

		void Commit() { m_HasHistory = true; }
		void Reset() { m_HasHistory = false; }

		// 한 프레임에 이보다 멀리 움직이면 다른 곳을 보는 것으로 여긴다(m).
		static constexpr f32 MaxCameraStep = 20.0f;

	private:
		// 같은 카메라라도 계산 오차로 마지막 자리가 달라질 수 있다.
		static bool IsSameView(const mat4& a, const mat4& b)
		{
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					const f32 difference = glm::abs(a[column][row] - b[column][row]);
					if (!(difference <= 1e-5f * glm::max(1.0f, glm::abs(b[column][row])))) return false;
				}
			}
			return true;
		}

		mat4 m_PrevViewProjection{ 1.0f };
		vec3 m_PrevCameraPosition{ 0.0f };
		const void* m_Scene = nullptr;
		uint32 m_Width = 0;
		uint32 m_Height = 0;
		uint32 m_NextWriteIndex = 0;
		uint32 m_FrameIndex = 0;
		bool m_HasHistory = false;
	};
}

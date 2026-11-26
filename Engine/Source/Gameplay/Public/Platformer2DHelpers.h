#pragma once

#include "Engine_Includes.h"

namespace Engine
{
	/**
	 * @brief 2D 플랫포머용 코요테 타임 및 점프 버퍼링 상태 컨트롤러
	 */
	class ENGINE_API PlatformerJumpController
	{
	public:
		explicit PlatformerJumpController(f32 coyoteDuration = 0.12f, f32 jumpBufferDuration = 0.12f) noexcept;

		void Update(bool isGrounded, f32 deltaTime) noexcept;
		void RequestJump() noexcept;
		[[nodiscard]] bool ConsumeJump() noexcept;
		[[nodiscard]] bool CanJump() const noexcept;
		void Reset() noexcept;

		void SetCoyoteDuration(f32 duration) noexcept;
		[[nodiscard]] f32 GetCoyoteDuration() const noexcept { return m_CoyoteDuration; }

		void SetJumpBufferDuration(f32 duration) noexcept;
		[[nodiscard]] f32 GetJumpBufferDuration() const noexcept { return m_JumpBufferDuration; }

		[[nodiscard]] f32 GetCoyoteTimeRemaining() const noexcept { return m_CoyoteTimer; }
		[[nodiscard]] f32 GetJumpBufferRemaining() const noexcept { return m_JumpBufferTimer; }
		[[nodiscard]] bool HasBufferedJump() const noexcept { return m_HasBufferedJump; }
		[[nodiscard]] bool IsGrounded() const noexcept { return m_IsGrounded; }

	private:
		f32 m_CoyoteDuration = 0.12f;
		f32 m_JumpBufferDuration = 0.12f;
		f32 m_CoyoteTimer = 0.0f;
		f32 m_JumpBufferTimer = 0.0f;
		bool m_IsGrounded = false;
		bool m_HasBufferedJump = false;
	};

	/**
	 * @brief 2D 원웨이(단방향) 플랫폼 통과/착지 판정 유틸리티
	 */
	class ENGINE_API OneWayPlatform2D
	{
	public:
		[[nodiscard]] static bool ShouldCollide(f32 prevFootY, f32 currFootY, f32 velocityY, f32 platformTopY,
			bool dropThroughRequested, f32 tolerance = 0.05f) noexcept;
	};

	/**
	 * @brief 2D 룸 경계 기준 카메라 위치 클램핑 유틸리티
	 */
	class ENGINE_API Camera2DRoomBounds
	{
	public:
		[[nodiscard]] static vec2 ClampPosition(const vec2& desiredPos, const vec2& halfViewSize,
			const vec2& roomMin, const vec2& roomMax) noexcept;
	};
}

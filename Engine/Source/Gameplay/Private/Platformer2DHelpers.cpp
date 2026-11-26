#include "Platformer2DHelpers.h"

namespace Engine
{
	namespace Platformer2DDetail
	{
		f32 ClampAxisToRoom(f32 desired, f32 halfView, f32 roomMin, f32 roomMax) noexcept
		{
			const f32 minBound = std::min(roomMin, roomMax);
			const f32 maxBound = std::max(roomMin, roomMax);
			const f32 safeHalfView = std::max(0.0f, halfView);

			const f32 lower = minBound + safeHalfView;
			const f32 upper = maxBound - safeHalfView;

			if (lower >= upper)
			{
				return 0.5f * (minBound + maxBound);
			}

			return std::clamp(desired, lower, upper);
		}
	}

	PlatformerJumpController::PlatformerJumpController(f32 coyoteDuration, f32 jumpBufferDuration) noexcept
		: m_CoyoteDuration(std::max(0.0f, coyoteDuration))
		, m_JumpBufferDuration(std::max(0.0f, jumpBufferDuration))
	{
	}

	void PlatformerJumpController::Update(bool isGrounded, f32 deltaTime) noexcept
	{
		const f32 dt = std::max(0.0f, deltaTime);
		m_IsGrounded = isGrounded;

		if (isGrounded)
		{
			m_CoyoteTimer = m_CoyoteDuration;
		}
		else
		{
			m_CoyoteTimer = std::max(0.0f, m_CoyoteTimer - dt);
		}

		if (m_HasBufferedJump)
		{
			m_JumpBufferTimer = std::max(0.0f, m_JumpBufferTimer - dt);
			if (m_JumpBufferTimer <= 0.0f && (dt > 0.0f || m_JumpBufferDuration > 0.0f))
			{
				m_HasBufferedJump = false;
			}
		}
	}

	void PlatformerJumpController::RequestJump() noexcept
	{
		m_JumpBufferTimer = m_JumpBufferDuration;
		m_HasBufferedJump = true;
	}

	bool PlatformerJumpController::ConsumeJump() noexcept
	{
		if (!CanJump() || !m_HasBufferedJump)
		{
			return false;
		}

		m_CoyoteTimer = 0.0f;
		m_JumpBufferTimer = 0.0f;
		m_HasBufferedJump = false;
		m_IsGrounded = false;
		return true;
	}

	bool PlatformerJumpController::CanJump() const noexcept
	{
		return m_IsGrounded || m_CoyoteTimer > 0.0f;
	}

	void PlatformerJumpController::Reset() noexcept
	{
		m_CoyoteTimer = 0.0f;
		m_JumpBufferTimer = 0.0f;
		m_IsGrounded = false;
		m_HasBufferedJump = false;
	}

	void PlatformerJumpController::SetCoyoteDuration(f32 duration) noexcept
	{
		m_CoyoteDuration = std::max(0.0f, duration);
		m_CoyoteTimer = std::min(m_CoyoteTimer, m_CoyoteDuration);
	}

	void PlatformerJumpController::SetJumpBufferDuration(f32 duration) noexcept
	{
		m_JumpBufferDuration = std::max(0.0f, duration);
		m_JumpBufferTimer = std::min(m_JumpBufferTimer, m_JumpBufferDuration);
	}

	bool OneWayPlatform2D::ShouldCollide(f32 prevFootY, f32 currFootY, f32 velocityY, f32 platformTopY,
		bool dropThroughRequested, f32 tolerance) noexcept
	{
		if (dropThroughRequested || velocityY > 0.0f)
		{
			return false;
		}

		const f32 safeTolerance = std::max(0.0f, tolerance);
		const bool wasAboveOrOnPlatform = prevFootY >= (platformTopY - safeTolerance);
		const bool reachedOrBelowTop = currFootY <= (platformTopY + safeTolerance);

		return wasAboveOrOnPlatform && reachedOrBelowTop;
	}

	vec2 Camera2DRoomBounds::ClampPosition(const vec2& desiredPos, const vec2& halfViewSize,
		const vec2& roomMin, const vec2& roomMax) noexcept
	{
		return vec2(
			Platformer2DDetail::ClampAxisToRoom(desiredPos.x, halfViewSize.x, roomMin.x, roomMax.x),
			Platformer2DDetail::ClampAxisToRoom(desiredPos.y, halfViewSize.y, roomMin.y, roomMax.y)
		);
	}
}

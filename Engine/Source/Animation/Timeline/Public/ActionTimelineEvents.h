#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Delegate.h"
#include <entt/entity/registry.hpp>
#include <cstddef>
#include <functional>
#include <string>

BEGIN(Engine)
class Scene;

// 액션 타임라인의 이벤트 노티파이(CustomEvent)가 게임 실행 중 지정 시점을 지날 때 게임플레이로 보내는 데이터.
// 핸들러 호출 동안만 유효하다. scene 포인터를 보관하지 말고, 엔티티는 호출 시점에 유효하다.
struct ActionTimelineEvent
{
	Scene* scene = nullptr;
	entt::entity entity = entt::null; // ActionTimelineComponent를 가진 엔티티
	std::string eventTag;
	std::string notifyName;
	float triggerTime = 0.0f;
};

// 액션 이벤트 구독 창구. 리스너 목록은 엔진 DLL이 소유한다.
// 핫리로드되는 모듈은 OnLoad에서 구독하고 OnUnload에서 반드시 해제한다. 해제하지 않으면 언로드된 코드를 가리키는 핸들러가 남는다.
// ActionTimelineSystem이 씬 단위로 타임라인 갱신을 마친 뒤 같은 프레임에 발행하므로, 핸들러에서 엔티티를 만들거나 지워도 된다.
class ENGINE_API ActionTimelineEvents
{
public:
	using Handler = std::function<void(const ActionTimelineEvent&)>;

	// 빈 핸들러는 등록하지 않고 0을 돌려준다.
	[[nodiscard]] static DelegateHandle Subscribe(Handler handler);
	static void Unsubscribe(DelegateHandle handle);
	static void Publish(const ActionTimelineEvent& event);
	static std::size_t GetSubscriberCount();
};
END

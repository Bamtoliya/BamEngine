#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Delegate.h"
#include "ActionTimelinePlayback.h"
#include <entt/entity/registry.hpp>
#include <cstddef>
#include <cstdint>
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
	std::uint32_t playSerial = 0; // 노티파이를 발생시킨 재생 세션
};

// 재생 세션이 끝났을 때 보내는 데이터. 핸들러 호출 동안만 유효하다.
// Stopped·Interrupted는 세션 번호가 이미 바뀐 뒤 판정하므로 playSerial은 끝난 세션의 번호다(현재 컴포넌트 값과 다를 수 있다).
struct ActionTimelineEndEvent
{
	Scene* scene = nullptr;
	entt::entity entity = entt::null; // ActionTimelineComponent를 가진 엔티티
	EActionTimelineEndReason reason = EActionTimelineEndReason::Completed;
	std::uint32_t playSerial = 0; // 끝난 재생 세션
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

	// 재생 종료 구독. 노티파이 이벤트와 저장소가 달라 핸들을 섞어 해제하면 안 된다. 수명 규칙은 Subscribe와 같다.
	using EndHandler = std::function<void(const ActionTimelineEndEvent&)>;
	[[nodiscard]] static DelegateHandle SubscribeEnd(EndHandler handler);
	static void UnsubscribeEnd(DelegateHandle handle);
	static void PublishEnd(const ActionTimelineEndEvent& event);
	static std::size_t GetEndSubscriberCount();
};
END

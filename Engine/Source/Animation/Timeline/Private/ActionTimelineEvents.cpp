#include "ActionTimelineEvents.h"

namespace
{
	// 리스너 저장소를 엔진 DLL 안에 둔다. 모듈 DLL이 템플릿을 인스턴스화해 저장소를 만들면 언로드 후 저장소 정리 코드가 사라지기 때문이다.
	Engine::MulticastDelegate<const Engine::ActionTimelineEvent&>& GetActionTimelineEventListeners()
	{
		static Engine::MulticastDelegate<const Engine::ActionTimelineEvent&> listeners;
		return listeners;
	}
}

Engine::DelegateHandle Engine::ActionTimelineEvents::Subscribe(Handler handler)
{
	if (!handler) return 0;
	return GetActionTimelineEventListeners().Add(std::move(handler));
}

void Engine::ActionTimelineEvents::Unsubscribe(DelegateHandle handle)
{
	if (handle != 0) GetActionTimelineEventListeners().Remove(handle);
}

void Engine::ActionTimelineEvents::Publish(const ActionTimelineEvent& event)
{
	GetActionTimelineEventListeners().Broadcast(event);
}

std::size_t Engine::ActionTimelineEvents::GetSubscriberCount()
{
	return GetActionTimelineEventListeners().GetListenerCount();
}

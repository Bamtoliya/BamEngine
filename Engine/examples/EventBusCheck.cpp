#include "EventBus.h"
#include "Delegate.h"

#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>

namespace
{
    using namespace Engine;

    struct TestEvent
    {
        int Value = 0;
    };

    struct OtherEvent
    {
        int Value = 0;
    };

    bool Check(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "[FAIL] " << message << std::endl;

        return condition;
    }

    // 자기 자신과 다른 콜백을 해제하고 신규 콜백을 등록한다.
    bool CheckListenerMutation()
    {
        MulticastDelegate<> signal;
        std::vector<int> calls;

        DelegateHandle first = 0;
        DelegateHandle second = 0;

        first = signal.AddLambda([&]()
            {
                calls.push_back(1);

                signal.Remove(first);
                signal.Remove(second);

                signal.AddLambda([&]() { calls.push_back(3); });
            });

        second = signal.AddLambda([&]() { calls.push_back(2); });

        signal.Broadcast();

        if (!Check(calls == std::vector<int>{ 1 }, "Mutation during broadcast failed."))
            return false;

        signal.Broadcast();

        return Check(calls == std::vector<int>{ 1, 3 },
            "New listener must execute on the next broadcast.");
    }

    // 발행 도중 모든 구독을 제거한다.
    bool CheckClearDuringBroadcast()
    {
        MulticastDelegate<> signal;
        int calls = 0;

        signal.AddLambda([&]()
            {
                ++calls;
                signal.Clear();
            });

        signal.AddLambda([&]() { calls += 100; });

        signal.Broadcast();

        return Check(calls == 1 && !signal.IsBound() && signal.GetListenerCount() == 0,
            "Clear must disconnect all remaining listeners.");
    }

    // mutable lambda의 상태가 Broadcast 사이에 유지되어야 한다.
    bool CheckMutableCallback()
    {
        MulticastDelegate<> signal;
        int result = 0;

        signal.AddLambda([count = 0, &result]() mutable
            {
                result = ++count;
            });

        signal.Broadcast();
        signal.Broadcast();
        signal.Broadcast();

        return Check(result == 3, "Mutable callback state was lost.");
    }

    // 복사본의 구독 해제가 원본의 연결 상태를 변경하면 안 된다.
    bool CheckDelegateCopy()
    {
        MulticastDelegate<> original;
        int result = 0;

        const auto handle = original.AddLambda([count = 0, &result]() mutable
            {
                result = ++count;
            });

        MulticastDelegate<> copied = original;

        original.Broadcast();
        copied.Broadcast();

        if (!Check(result == 2, "Multicast copy must preserve shared callable state."))
            return false;

        copied.Remove(handle);

        if (!Check(original.IsBound() && !copied.IsBound(), "Copied subscriptions must be independent."))
            return false;

        original.Broadcast();
        copied.Broadcast();

        return Check(result == 3, "Removing a copied subscription affected the original.");
    }

    // 단일 Delegate가 실행 중 자기 자신을 해제해도 callable은 살아 있어야 한다.
    bool CheckSingleDelegateUnbind()
    {
        Delegate<int> signal;

        auto payload = std::make_shared<int>(42);
        std::weak_ptr<int> weak = payload;

        signal.Bind([&signal, payload]()
            {
                signal.Unbind();

                return *payload;
            });

        payload.reset();

        if (!Check(signal.Execute() == 42, "Self-unbind destroyed the active callable."))
            return false;

        if (!Check(!signal.IsBound() && weak.expired(), "Unbound callable was retained."))
            return false;

        bool threw = false;

        try
        {
            signal.Execute();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        return Check(threw, "Executing an unbound delegate must throw.");
    }

    // 단일 Delegate가 실행 중 새로운 함수로 교체될 수 있어야 한다.
    bool CheckSingleDelegateRebind()
    {
        Delegate<int> signal;

        signal.Bind([&signal]()
            {
                signal.Bind([]() { return 200; });

                return 100;
            });

        if (!Check(signal.Execute() == 100, "Rebind replaced the active invocation."))
            return false;

        return Check(signal.Execute() == 200, "Rebound function was not executed.");
    }

    // 발행 도중 Reset 후 같은 이벤트를 다시 구독한다.
    bool CheckEventBusReset()
    {
        EventBus bus;
        std::vector<int> calls;

        DelegateHandle replacement = 0;

        const auto oldHandle = bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                calls.push_back(1);

                bus.Reset();

                replacement = bus.Subscribe<TestEvent>([&](const TestEvent&)
                    {
                        calls.push_back(3);
                    });
            });

        bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                calls.push_back(2);
            });

        bus.Publish(TestEvent{});

        if (!Check(calls == std::vector<int>{ 1 }, "Reset must stop old listeners."))
            return false;

        if (!Check(replacement != 0 && replacement != oldHandle, "Reset reused an old subscription handle."))
            return false;

        bus.Unsubscribe<TestEvent>(oldHandle);
        bus.Publish(TestEvent{});

        return Check(calls == std::vector<int>{ 1, 3 },
            "A stale subscription handle removed the new listener.");
    }

    // 중첩 Publish는 현재 시점의 구독 상태를 사용한다.
    bool CheckNestedPublish()
    {
        EventBus bus;
        std::vector<int> calls;

        DelegateHandle second = 0;

        bus.Subscribe<TestEvent>([&](const TestEvent& event)
            {
                calls.push_back(event.Value);

                if (event.Value == 1)
                {
                    bus.Unsubscribe<TestEvent>(second);

                    bus.Subscribe<TestEvent>([&](const TestEvent&)
                        {
                            calls.push_back(3);
                        });

                    bus.Publish(TestEvent{ 10 });
                }
            });

        second = bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                calls.push_back(2);
            });

        bus.Publish(TestEvent{ 1 });

        return Check(calls == std::vector<int>{ 1, 10, 3 },
            "Nested publish did not use the current subscription state.");
    }

    // 서로 다른 이벤트의 중첩 발행 중 Reset을 수행한다.
    bool CheckNestedReset()
    {
        EventBus bus;
        std::vector<int> calls;

        bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                calls.push_back(1);

                bus.Publish(OtherEvent{});

                calls.push_back(4);
            });

        bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                calls.push_back(99);
            });

        bus.Subscribe<OtherEvent>([&](const OtherEvent&)
            {
                calls.push_back(2);

                bus.Reset();

                bus.Subscribe<TestEvent>([&](const TestEvent&)
                    {
                        calls.push_back(3);
                    });

                bus.Publish(TestEvent{});
            });

        bus.Subscribe<OtherEvent>([&](const OtherEvent&)
            {
                calls.push_back(98);
            });

        bus.Publish(TestEvent{});

        if (!Check(calls == std::vector<int>{ 1, 2, 3, 4 }, "Nested Reset invalidated dispatch."))
            return false;

        bus.Publish(TestEvent{});

        return Check(calls == std::vector<int>{ 1, 2, 3, 4, 3 },
            "New event storage was not preserved after nested Reset.");
    }

    // 특정 이벤트만 제거해야 한다.
    bool CheckTypedClear()
    {
        EventBus bus;
        int first = 0;
        int second = 0;

        bus.Subscribe<TestEvent>([&](const TestEvent&) { ++first; });
        bus.Subscribe<OtherEvent>([&](const OtherEvent&) { ++second; });

        bus.Clear<TestEvent>();

        bus.Publish(TestEvent{});
        bus.Publish(OtherEvent{});

        return Check(first == 0 && second == 1, "Typed Clear affected an unrelated event.");
    }

    // 콜백 예외가 발생한 이후에도 Delegate 상태가 정상이어야 한다.
    bool CheckCallbackException()
    {
        EventBus bus;
        int calls = 0;

        DelegateHandle throwing = 0;

        throwing = bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                bus.Unsubscribe<TestEvent>(throwing);

                throw std::runtime_error("Expected callback exception.");
            });

        bus.Subscribe<TestEvent>([&](const TestEvent&)
            {
                ++calls;
            });

        bool caught = false;

        try
        {
            bus.Publish(TestEvent{});
        }
        catch (const std::runtime_error&)
        {
            caught = true;
        }

        if (!Check(caught && calls == 0, "Callback exception was not propagated."))
            return false;

        bus.Publish(TestEvent{});

        return Check(calls == 1, "EventBus was corrupted after a callback exception.");
    }

    // 비어 있는 콜백은 등록하지 않는다.
    bool CheckEmptyCallbacks()
    {
        EventBus bus;
        MulticastDelegate<> signal;

        if (!Check(signal.Add({}) == 0 && signal.GetListenerCount() == 0,
            "Empty multicast callback was registered."))
        {
            return false;
        }

        return Check(bus.Subscribe<TestEvent>({}) == 0, "Empty event callback was registered.");
    }
}

int main()
{
    const auto run = [](const char* name, auto&& check)
        {
            std::cerr << "[RUN] " << name << std::endl;

            const bool result = check();

            std::cerr << (result ? "[PASS] " : "[FAIL] ") << name << std::endl;

            return result;
        };

    if (!run("Listener mutation", CheckListenerMutation)) return 1;
    if (!run("Clear during broadcast", CheckClearDuringBroadcast)) return 1;
    if (!run("Mutable callback", CheckMutableCallback)) return 1;
    if (!run("Delegate copy", CheckDelegateCopy)) return 1;
    if (!run("Single delegate unbind", CheckSingleDelegateUnbind)) return 1;
    if (!run("Single delegate rebind", CheckSingleDelegateRebind)) return 1;
    if (!run("EventBus reset", CheckEventBusReset)) return 1;
    if (!run("Nested publish", CheckNestedPublish)) return 1;
    if (!run("Nested reset", CheckNestedReset)) return 1;
    if (!run("Typed clear", CheckTypedClear)) return 1;
    if (!run("Callback exception", CheckCallbackException)) return 1;
    if (!run("Empty callbacks", CheckEmptyCallbacks)) return 1;

    std::cout << "All EventBusCheck tests passed." << std::endl;

    return 0;
}
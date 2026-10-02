#pragma once

#include <reflection/Metadata.h>
#include <reflection/Invocation.h>

#include <any>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <optional>

namespace reflection
{
    namespace detail
    {
        template<typename T>
        struct SupportedMember : std::false_type
        {
        };

        template<typename Result, typename Owner, typename... Args>
        struct SupportedMember<Result(Owner::*)(Args...)>
            : std::bool_constant<
            (
                std::is_void_v<Result> ||
                (
                    std::is_scalar_v<Result> &&
                    !std::is_const_v<Result> &&
                    !std::is_volatile_v<Result>
                    )
                ) &&
            (!std::is_reference_v<Args> && ...)
            >
        {
        };

        template<typename Result, typename Owner, typename... Args>
        struct SupportedMember<Result(Owner::*)(Args...) const>
            : SupportedMember<Result(Owner::*)(Args...)>
        {
        };

        template<typename T>
        struct SupportedCallable : SupportedMember<T>
        {
        };

        template<typename Result, typename... Args>
        struct SupportedCallable<Result(*)(Args...)>
            : std::bool_constant<
            (std::is_void_v<Result> ||
                (std::is_scalar_v<Result> &&
                    !std::is_const_v<Result> && !std::is_volatile_v<Result>)) &&
            (!std::is_reference_v<Args> && ...)>
        {
        };
    }

    struct ParameterInfo
    {
        // 이름 없는 매개변수도 허용합니다.
        std::string Name;
        std::string TypeName;
    };

    struct FunctionInfo;

    template<auto Member>
    FunctionInfo MakeFunction();

    struct FunctionInfo
    {
        std::string Name;
        std::string ReturnType;
        std::string Signature;

        bool IsStaticMember = false;
        bool IsConstMember = false;

        std::vector<ParameterInfo> Parameters;
        reflection::Metadata Metadata;

        [[nodiscard]]
        bool CanInvoke() const
        {
            return m_callable.has_value();
        }

        [[nodiscard]]
        InvocationBindingStatus GetBindingStatus() const
        {
            return CanInvoke() ? InvocationBindingStatus::Available : m_bindingStatus;
        }

        [[nodiscard]]
        static FunctionInfo DescribeOnly(InvocationBindingStatus status)
        {
            FunctionInfo function;
            function.m_bindingStatus = status == InvocationBindingStatus::Available
                ? InvocationBindingStatus::NotBound : status;
            return function;
        }

        template<typename Owner, typename... Args>
            requires (
        std::is_class_v<Owner> &&
            !std::is_volatile_v<Owner> &&
            (!std::is_reference_v<Args> && ...)
            )
            [[nodiscard]]
        bool Invoke(Owner& object, Args... args) const
        {
            using ObjectType = std::remove_const_t<Owner>;

            // 변경 가능한 객체에서는 일반 멤버 함수도 호출합니다.
            if constexpr (!std::is_const_v<Owner>)
            {
                using MemberPointer = void (ObjectType::*)(Args...);

                const MemberPointer* member =
                    std::any_cast<MemberPointer>(&m_callable);

                if (member != nullptr)
                {
                    (object.*(*member))(std::move(args)...);
                    return true;
                }
            }

            // const 멤버 함수는 일반 객체와 const 객체 모두에서 호출합니다.
            using ConstMemberPointer =
                void (ObjectType::*)(Args...) const;

            const ConstMemberPointer* member =
                std::any_cast<ConstMemberPointer>(&m_callable);

            if (member == nullptr)
            {
                return false;
            }

            (object.*(*member))(std::move(args)...);
            return true;
        }

        template<typename Result, typename Owner, typename... Args>
            requires (
        std::is_scalar_v<Result> &&
            !std::is_const_v<Result> &&
            !std::is_volatile_v<Result>&&
            std::is_class_v<Owner> &&
            !std::is_volatile_v<Owner> &&
            (!std::is_reference_v<Args> && ...)
            )
            [[nodiscard]]
        std::optional<Result> InvokeValue(
            Owner& object,
            Args... args
        ) const
        {
            using ObjectType = std::remove_const_t<Owner>;

            if constexpr (!std::is_const_v<Owner>)
            {
                using MemberPointer =
                    Result(ObjectType::*)(Args...);

                const MemberPointer* member =
                    std::any_cast<MemberPointer>(&m_callable);

                if (member != nullptr)
                {
                    return std::optional<Result>{
                        (object.*(*member))(std::move(args)...)
                    };
                }
            }

            using ConstMemberPointer =
                Result(ObjectType::*)(Args...) const;

            const ConstMemberPointer* member =
                std::any_cast<ConstMemberPointer>(&m_callable);

            if (member == nullptr)
            {
                return std::nullopt;
            }

            return std::optional<Result>{
                (object.*(*member))(std::move(args)...)
            };
        }

        template<typename... Args>
            requires ((!std::is_reference_v<Args> && ...))
        [[nodiscard]]
        bool InvokeWithoutObject(Args... args) const
        {
            using FunctionPointer = void (*)(Args...);
            const auto* function = std::any_cast<FunctionPointer>(&m_callable);

            if (function == nullptr)
            {
                return false;
            }

            (*function)(std::move(args)...);
            return true;
        }

        template<typename Result, typename... Args>
            requires (
        std::is_scalar_v<Result> &&
            !std::is_const_v<Result> && !std::is_volatile_v<Result> &&
            (!std::is_reference_v<Args> && ...))
            [[nodiscard]]
        std::optional<Result> InvokeValueWithoutObject(Args... args) const
        {
            using FunctionPointer = Result(*)(Args...);
            const auto* function = std::any_cast<FunctionPointer>(&m_callable);

            if (function == nullptr)
            {
                return std::nullopt;
            }

            return std::optional<Result>{(*function)(std::move(args)...)};
        }

    private:
        std::any m_callable;
        
        InvocationBindingStatus m_bindingStatus = InvocationBindingStatus::NotBound;

        template<auto Member>
        friend FunctionInfo MakeFunction();
    };

    template<auto Member>
    FunctionInfo MakeFunction()
    {
        static_assert(
            detail::SupportedCallable<decltype(Member)>::value,
            "Only supported member or ordinary function pointers are allowed"
            );

        static_assert(Member != nullptr, "Member must not be null");

        FunctionInfo function;
        function.m_callable = Member;
        return function;
    }
}
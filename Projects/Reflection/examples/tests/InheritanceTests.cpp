#include "TestSupport.h"

#include <reflection/Reflection.h>

#include <cstdint>
#include <utility>

namespace reflection_tests
{
    namespace
    {
        struct Base { int Id = 7; };
        struct Prefix { std::uint64_t Padding[4]{}; };
        struct Derived : Prefix, Base { int Health = 100; };
        struct Left : virtual Base {};
        struct Right : virtual Base {};
        struct Diamond : Left, Right {};
        class Hidden : private Base
        {
        public:
            static reflection::BaseInfo Binding()
            {
                return reflection::BaseInfo::Bind<Hidden, Base,
                    [](const Hidden& value) -> const Base& { return static_cast<const Base&>(value); },
                    [](Hidden& value) -> Base& { return static_cast<Base&>(value); }>("Base");
            }
        };
    }

    bool ValidateInheritance()
    {
        using Error = reflection::RegistrationError;
        const auto base = reflection::BaseInfo::For<Derived, Base>("Base");
        const auto id = reflection::MakeProperty<&Base::Id>("Id", "int");
        Derived object;
        const auto source = reflection::ObjectView::From(object);
        const auto converted = base.AsBase(source);
        if (!base.CanCast() || !converted.Is<Base>() || converted.IsReadOnly() ||
            !converted.RefersToSameObject(reflection::ObjectView::From(static_cast<Base&>(object))) ||
            !id.WriteTo(converted, 31) || object.Id != 31 || base.AsBase(converted).IsValid() ||
            base.AsBase({}).IsValid() ||
            id.TryReadValue(source).Error != reflection::PropertyAccessError::OwnerTypeMismatch)
        {
            return false;
        }
        const auto readOnly = base.AsBase(reflection::ObjectView::From(std::as_const(object)));
        if (!readOnly.IsReadOnly() || !readOnly.RefersToSameObject(converted) ||
            id.ReadFrom<int>(readOnly) == nullptr || *id.ReadFrom<int>(readOnly) != 31 || id.WriteTo(readOnly, 99) ||
            object.Id != 31 || reflection::ObjectView{}.RefersToSameObject({}))
        {
            return false;
        }
        Diamond diamond;
        const auto diamondView = reflection::ObjectView::From(diamond);
        const auto left = reflection::BaseInfo::For<Diamond, Left>("Left").AsBase(diamondView);
        const auto right = reflection::BaseInfo::For<Diamond, Right>("Right").AsBase(diamondView);
        const auto leftBase = reflection::BaseInfo::For<Left, Base>("Base", true).AsBase(left);
        const auto rightBase = reflection::BaseInfo::For<Right, Base>("Base", true).AsBase(right);
        if (!leftBase.RefersToSameObject(rightBase) || !id.WriteTo(leftBase, 19) || diamond.Id != 19)
        {
            return false;
        }
        Hidden hidden;
        const auto inaccessible = reflection::BaseInfo::For<Hidden, Base>("Base");
        const auto bound = Hidden::Binding();
        if (inaccessible.CanCast() || !bound.CanCast() ||
            !id.WriteTo(bound.AsBase(reflection::ObjectView::From(hidden)), 13) ||
            !bound.AsBase(reflection::ObjectView::From(std::as_const(hidden))).IsReadOnly())
        {
            return false;
        }

        reflection::Registry registry;
        auto wrongOwner = reflection::TypeInfo::For<Base>("WrongOwner");
        wrongOwner.Bases.push_back(base);
        auto unavailable = reflection::TypeInfo::For<Hidden>("Hidden");
        unavailable.Bases.push_back(inaccessible);
        auto duplicate = reflection::TypeInfo::For<Derived>("Duplicate");
        duplicate.Bases = { base, base };
        auto unbound = reflection::TypeInfo::For<Derived>("Unbound");
        unbound.Bases.push_back({});
        if (registry.TryRegister(std::move(wrongOwner)).Error != Error::InvalidBase ||
            registry.TryRegister(std::move(unavailable)).Error != Error::InvalidBase ||
            registry.TryRegister(std::move(duplicate)).Error != Error::DuplicateBase ||
            registry.TryRegister(std::move(unbound)).Error != Error::InvalidBase ||
            registry.FindType<Derived>() != nullptr || registry.FindType<Base>() != nullptr)
        {
            return false;
        }
        auto valid = reflection::TypeInfo::For<Derived>("Derived");
        valid.Bases.push_back(base);
        auto baseType = reflection::TypeInfo::For<Base>("Base");
        baseType.Properties.push_back(id);
        return registry.Register(std::move(valid)) && registry.FindType<Base>() == nullptr &&
            registry.Register(std::move(baseType)) && registry.FindType<Derived>()->Bases.size() == 1;
    }
}

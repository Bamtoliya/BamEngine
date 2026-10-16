#include <reflection/Reflection.h>

#include <iostream>
#include <utility>
#include <vector>

namespace
{
    struct Identity { int Id = 7; };
    struct Settings : Identity { std::vector<std::vector<int>> Rows{ { 1, 2 } }; };
}

int main()
{
    auto identityType = reflection::TypeInfo::For<Identity>("Identity");
    identityType.Properties.push_back(reflection::MakeProperty<&Identity::Id>("Id", "int"));
    auto settingsType = reflection::TypeInfo::For<Settings>("Settings");
    settingsType.Bases.push_back(reflection::BaseInfo::For<Settings, Identity>("Identity"));
    settingsType.Properties.push_back(reflection::MakeProperty<&Settings::Rows>("Rows", "Rows"));
    reflection::Registry registry;
    if (!registry.Register(std::move(settingsType)) || !registry.Register(std::move(identityType)))
    {
        std::cerr << "Runtime-only package registration failed.\n";
        return 1;
    }

    const auto* type = registry.FindType<Settings>();
    const auto* baseType = registry.FindType<Identity>();
    if (type == nullptr || baseType == nullptr || type->Bases.size() != 1)
    {
        return 1;
    }
    const auto* id = baseType->FindProperty("Id");
    const auto* rows = type->FindProperty("Rows");
    if (id == nullptr || rows == nullptr)
    {
        return 1;
    }
    Settings settings;
    const auto object = reflection::ObjectView::From(settings);
    const auto base = type->Bases[0].AsBase(object);
    if (!id->WriteTo(base, 31) || settings.Id != 31 ||
        id->TryReadValue(object).Error != reflection::PropertyAccessError::OwnerTypeMismatch)
    {
        std::cerr << "Runtime-only package inheritance access failed.\n";
        return 1;
    }

    const auto* rowType = rows->GetElementContainerProperty();
    const auto row = rows->TryEditElementObject(object, 0);
    const int appended = 9;
    const auto value = reflection::ValueView::From(appended);
    if (rowType == nullptr || !row ||
        rowType->TryAppend(row.Object, value) != reflection::PropertyAccessError::None ||
        settings.Rows[0] != std::vector<int>{ 1, 2, 9 })
    {
        std::cerr << "Runtime-only package nested container access failed.\n";
        return 1;
    }
    const auto readOnly = reflection::ObjectView::From(std::as_const(settings));
    const auto readOnlyRow = rows->TryReadElement(readOnly, 0);
    const auto readOnlyError = reflection::PropertyAccessError::ReadOnlyObject;
    if (!readOnlyRow || !type->Bases[0].AsBase(readOnly).IsReadOnly() ||
        rowType->TryAppend(rowType->AsObject(readOnlyRow.Value), value) != readOnlyError ||
        settings.Rows[0] != std::vector<int>{ 1, 2, 9 })
    {
        std::cerr << "Runtime-only package const access failed.\n";
        return 1;
    }
    std::cout << "Installed Reflection Runtime works without code generation or Archive.\n";
    return 0;
}

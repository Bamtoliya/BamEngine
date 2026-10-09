#include "Demo.gen.h"
#include "ExampleTypes.h"
#include <reflection_entt/Reflection.h>
#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
    template<typename Condition>
    void Check(Condition&& condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void RunChecks()
    {
        using namespace entt::literals;
        entt::meta_ctx context;
        reflection_entt::RegistrationResult registration;
        Check(reflection_entt_generated::Register_Demo(context, registration), "Registration failed");
        const auto settingsType = entt::resolve<example::MovementSettings>(context);
        const auto typeMetadata = reflection_entt::GetMetadata(settingsType);
        Check(typeMetadata && *typeMetadata->FindAs<std::string>("DisplayName") == "Movement Settings", "Type metadata");
        const auto speed = settingsType.data("Speed"_hs);
        const auto metadata = reflection_entt::GetMetadata(speed);
        Check(metadata && *metadata->FindAs<bool>("Editable"), "Boolean metadata");
        Check(*metadata->FindAs<double>("RangeMax") == 100.0, "Range metadata");
        Check(*metadata->FindAs<std::string>("Help") == "Speed, \"fast\"", "Expanded/escaped metadata");

        example::MovementSettings settings;
        auto instance = entt::forward_as_meta(context, settings);
        Check(speed.set(instance, 25.0f) && settings.Speed == 25.0f, "Property write");
        Check(&speed.get(instance).cast<float&>() == &settings.Speed, "Property read must reference the original");
        std::wstring title = L"ReflectionEnTT";
        Check(settingsType.data("Title"_hs).set(instance, title) && settings.Title == title, "wstring write");
        Check(!settingsType.data("Revision"_hs).set(instance, 9), "Const property must reject writes");

        auto constInstance = entt::forward_as_meta(context, std::as_const(settings));
        Check(!speed.set(constInstance, 99.0f) && settings.Speed == 25.0f, "Const object write");
        auto sequence = settingsType.data("Samples"_hs).get(instance).as_sequence_container();
        Check(sequence && sequence.resize(3) && settings.Samples.size() == 3, "Vector reference/resize");
        auto weights = settingsType.data("Weights"_hs).get(instance).as_associative_container();
        Check(weights && weights.size() == 1, "Map reflection");
        Check(weights.insert(std::string{"Run"}, 2.0f) && settings.Weights.at("Run") == 2.0f, "Map insertion");
        auto tags = settingsType.data("Tags"_hs).get(instance).as_associative_container();
        Check(tags && tags.size() == 1, "Set reflection");
        Check(tags.insert(std::string{"Editable"}, {}) && settings.Tags.contains("Editable"), "Set insertion");

        example::Player player;
        auto playerInstance = entt::forward_as_meta(context, player);
        const auto playerType = entt::resolve<example::Player>(context);
        Check(playerType.data("m_position"_hs).set(playerInstance, 2.0f), "Private property write");
        Check(playerType.invoke("Move"_hs, playerInstance, 3.0f), "Member invocation");
        Check(player.GetPosition() == 5.0f, "Member invocation result");
        Check(playerType.invoke("Move"_hs, playerInstance, 2.0f, 0.5f), "Overloaded invocation");
        Check(player.GetPosition() == 6.0f, "Overloaded invocation result");
        Check(playerType.invoke("GetPosition"_hs, playerInstance).cast<float>() == 6.0f, "Const/noexcept function");
        Check(playerType.invoke("Label"_hs, playerInstance).cast<std::string>() == "Player", "Object return");
        std::string label = "Updated";
        Check(playerType.invoke("SetLabel"_hs, playerInstance, entt::forward_as_meta(context, label)), "Reference argument");
        Check(playerType.invoke("LabelRef"_hs, playerInstance).cast<const std::string&>() == label, "Reference return");
        Check(playerType.invoke("Twice"_hs, {}, 4.0f).cast<float>() == 8.0f, "Static function");
        Check(playerType.data("Root"_hs).get(playerInstance).cast<int>() == 7, "Inherited property");

        std::array<entt::meta_any, 1> arguments{entt::meta_any{context, 1.5f}};
        auto move = playerType.func("Move"_hs);
        while (move && move.arity() != arguments.size()) move = move.next();
        Check(move && move.invoke(playerInstance, arguments.data(), arguments.size()), "Erased dynamic argument invocation");
        Check(player.GetPosition() == 7.5f, "Dynamic invocation result");
        Check(playerType.invoke("Reset"_hs, playerInstance) && player.GetPosition() == 0.0f, "Private function");

        const auto enumeration = entt::resolve<example::MovementMode>(context);
        Check(enumeration.data("Run"_hs).get({}).cast<example::MovementMode>() == example::MovementMode::Run, "Enum value");
        Check(enumeration.data("Active"_hs).get({}).cast<example::MovementMode>() == example::MovementMode::Run, "Enum alias");
        Check(enumeration.data("Maximum"_hs).get({}).cast<example::MovementMode>() == example::MovementMode::Maximum, "Unsigned enum maximum");
        Check(reflection_entt_generated::FreeFunctions_Demo(context)
            .invoke("example::DoubleValue"_hs, {}, 4.0f).cast<float>() == 8.0f, "Free function");

        auto constructed = settingsType.construct();
        Check(constructed && constructed.try_cast<example::MovementSettings>(), "Default construction");
        Check(!reflection_entt_generated::Register_Demo(context, registration), "Duplicate module registration");
        Check(registration.Error == reflection_entt::RegistrationError::DuplicateType, "Duplicate diagnosis");
        Check(speed.get(instance).cast<float>() == 25.0f, "Duplicate attempt changed existing registration");
        entt::meta_ctx other;
        Check(!entt::resolve(other, entt::type_id<example::Player>()), "Context leaked");
        Check(reflection_entt_generated::Register_Demo(other), "Second isolated context");

        entt::meta_ctx occupied;
        entt::meta_factory<example::Player>{occupied}.type("ExistingPlayer");
        Check(!reflection_entt_generated::Register_Demo(occupied, registration), "Pre-existing type overwritten");
        Check(!entt::resolve(occupied, entt::type_id<example::MovementSettings>()), "Preflight partially registered types");
        std::cout << "Metadata, properties, containers, enums, functions, inheritance and context checks passed.\n";
    }
}

int main()
{
    try
    {
        RunChecks();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ReflectionEnTT checks failed: " << error.what() << '\n';
        return 1;
    }
}

#include "Engine_Includes.h"
#include "ReflectionInspectorSupport.h"
#include "ReflectionPropertyDrawer.h"
#include "InputManager.h"
#include "Reflection/ReflectionRegistry.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include <imgui.h>
#include <archive/JsonArchive.h>
#include <iostream>
#include <array>
#include <limits>
#include <utility>
#include <cstdint>

namespace
{
    struct Scope
    {
        ~Scope()
        {
            if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
            Engine::ReflectionRegistry::Destroy();
            Engine::ResourceManager::Destroy();
            Engine::LocalizationManager::Destroy();
            Engine::InputManager::Destroy();
        }
    };

    bool Fail(const char* message) { std::cerr << message << '\n'; return false; }

    bool CheckPayloads()
    {
        const std::wstring path = L"Resources/Texture/uv1.png";
        const int bytes = static_cast<int>((path.size() + 1) * sizeof(wchar_t));
        if (Editor::DecodeInspectorResourcePath(path.c_str(), bytes) != path ||
            Editor::DecodeInspectorResourcePath(nullptr, bytes) ||
            Editor::DecodeInspectorResourcePath(path.c_str(), bytes - 1) ||
            Editor::DecodeInspectorResourcePath(path.c_str(), bytes - sizeof(wchar_t)))
            return Fail("Malformed resource paths were accepted.");
        std::vector<char> unaligned(bytes + 1);
        std::memcpy(unaligned.data() + 1, path.c_str(), bytes);
        if (Editor::DecodeInspectorResourcePath(unaligned.data() + 1, bytes) != path)
            return Fail("Byte-buffer path decoding assumed wchar_t alignment.");
        return true;
    }

    bool CheckResourceTypes()
    {
        auto& manager = Engine::ResourceManager::Get();
        auto texture = manager.AddResource<Engine::Texture>(L"Resources/Texture/uv1.png",
            Engine::Texture::CreateEmpty());
        auto mesh = manager.AddResource<Engine::Mesh>(L"InspectorTestMesh", Engine::Mesh::CreateEmpty());
        auto material = manager.AddResource<Engine::Material>(L"InspectorTestMaterial",
            Engine::Material::CreateEmpty());
        auto instance = manager.AddResource<Engine::MaterialInstance>(L"InspectorTestInstance",
            Engine::MaterialInstance::CreateEmpty());
        Engine::ResourceHandle<Engine::Texture> selected;
        Engine::ResourceHandle<Engine::MaterialInterface> base;
        if (!Editor::TryAssignInspectorResource(selected, texture.GetRawHandle()) ||
            Editor::TryAssignInspectorResource(selected, mesh.GetRawHandle()) || selected != texture ||
            !Editor::TryAssignInspectorResource(base, material.GetRawHandle()) ||
            !Editor::TryAssignInspectorResource(base, instance.GetRawHandle()) ||
            Editor::TryAssignInspectorResource(base, texture.GetRawHandle()))
            return Fail("Resource assignment lost type safety or base-type compatibility.");
        return true;
    }

    bool CheckColliderValidation()
    {
        Engine::ColliderComponent original;
        original.ClearDirty();
        auto candidate = original;
        const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
        const auto* type = registry.FindType<Engine::ColliderComponent>();
        const auto* shape = type ? type->FindProperty("m_Shape") : nullptr;
        Engine::ColliderShape invalid = Engine::SphereShape{ -1.0f };
        if (!shape || shape->TryWriteValue(reflection::ObjectView::From(candidate),
            reflection::ValueView::From(invalid)) != reflection::PropertyAccessError::None)
            return Fail("Collider shape reflection is unavailable.");
        if (Editor::TryCommitInspectorCollider(original, candidate) || original.IsDirty() ||
            original.GetShapeType() != Engine::EColliderShapeType::Box)
            return Fail("Rejected collider edits changed the original.");
        candidate = original;
        if (!candidate.SetOffset({ 2.0f, 0.0f, 0.0f }) ||
            !Editor::TryCommitInspectorCollider(original, candidate) || !original.IsDirty())
            return Fail("Validated collider edits were not committed or marked dirty.");
        return true;
    }

    bool CheckJointArchiveBoundary()
    {
        auto& registry = Engine::ReflectionRegistry::Get();
        entt::registry world;
        const auto entity = world.create();
        world.emplace<Engine::JointComponent>(entity).ConnectedEntity = world.create();
        archive::JsonArchive writer;
        std::string before, after;
        if (!writer.Write("Keep", 7) || !writer.ToJson(before)) return false;
        const auto result = registry.WriteComponent<Engine::JointComponent>(writer, "Joint", world, entity);
        if (result.Code != archive::ArchiveErrorCode::UnsupportedType ||
            !writer.ToJson(after) || before != after)
            return Fail("Unresolved Joint references were silently omitted or corrupted the document.");
        return true;
    }

    bool CheckDragDrop(const reflection::Registry& registry)
    {
        struct Owner
        {
            Engine::ResourceHandle<Engine::Texture> Texture;
            Engine::ResourceHandle<Engine::Mesh> Mesh;
        };
        Owner owner;
        auto texture = reflection::MakeProperty<&Owner::Texture>("Texture", "ResourceHandle<Texture>");
        auto mesh = reflection::MakeProperty<&Owner::Mesh>("Mesh", "ResourceHandle<Mesh>");
        static_cast<void>(texture.Metadata.Add("Editable", true));
        static_cast<void>(mesh.Metadata.Add("Editable", true));
        const std::wstring path = L"Resources/Texture/uv1.png";
        auto& io = ImGui::GetIO();
        const auto frame = [&](const reflection::PropertyInfo& property, bool source, bool readOnly)
        {
            ImGui::NewFrame();
            if (source && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern))
            {
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path.c_str(),
                    (path.size() + 1) * sizeof(wchar_t));
                ImGui::EndDragDropSource();
            }
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f));
            ImGui::Begin("DragDropCheck");
            ImGui::SetCursorScreenPos(ImVec2(20.0f, 40.0f));
            const auto object = readOnly ? reflection::ObjectView::From(std::as_const(owner)) :
                reflection::ObjectView::From(owner);
            const bool changed = Editor::ReflectionPropertyDrawer::DrawValue(property, object, &registry);
            ImGui::End();
            ImGui::Render();
            return changed;
        };
        const auto drop = [&](const reflection::PropertyInfo& property, bool readOnly)
        {
            frame(property, false, readOnly);
            io.AddMousePosEvent(70.0f, 48.0f);
            io.AddMouseButtonEvent(0, true);
            frame(property, true, readOnly);
            frame(property, true, readOnly);
            io.AddMouseButtonEvent(0, false);
            return frame(property, true, readOnly);
        };
        if (!drop(texture, false) || !owner.Texture.IsValid())
            return Fail("Resource drag/drop did not assign a compatible loaded resource.");
        const auto selected = owner.Texture;
        if (drop(mesh, false) || owner.Mesh.IsValid() || owner.Texture != selected)
            return Fail("A resource drop accepted the wrong resource type.");
        owner.Texture.Reset();
        if (drop(texture, true) || owner.Texture.IsValid())
            return Fail("A resource drop modified a read-only object.");
        return true;
    }

    bool CheckInteractions(const reflection::Registry& registry)
    {
        Engine::LightComponent light;
        Engine::NameComponent name;
        const auto* lightType = registry.FindType<Engine::LightComponent>();
        const auto* nameType = registry.FindType<Engine::NameComponent>();
        const auto* enabled = lightType ? lightType->FindProperty("IsEnabled") : nullptr;
        if (!enabled || !nameType) return false;
        auto& io = ImGui::GetIO();
        ImVec2 clickPoint;
        float expandedHeight = 0.0f;
        const auto frame = [&](bool categories, bool readOnly)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f));
            ImGui::Begin("InteractionCheck");
            ImGui::SetCursorScreenPos(ImVec2(20.0f, 40.0f));
            bool changed = false;
            if (categories)
            {
                clickPoint = ImVec2(35.0f, 48.0f);
                Editor::ReflectionPropertyDrawer::DrawPropertyTable(*nameType,
                    reflection::ObjectView::From(name), &registry);
                expandedHeight = ImGui::GetCursorScreenPos().y - 40.0f;
            }
            else
            {
                const auto object = readOnly ? reflection::ObjectView::From(std::as_const(light)) :
                    reflection::ObjectView::From(light);
                changed = Editor::ReflectionPropertyDrawer::DrawValue(*enabled, object, &registry);
                const auto min = ImGui::GetItemRectMin();
                const auto max = ImGui::GetItemRectMax();
                clickPoint = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
            }
            ImGui::End();
            ImGui::Render();
            return changed;
        };
        const auto click = [&](bool categories, bool readOnly)
        {
            io.AddMousePosEvent(clickPoint.x, clickPoint.y);
            frame(categories, readOnly);
            io.AddMouseButtonEvent(0, true);
            frame(categories, readOnly);
            io.AddMouseButtonEvent(0, false);
            return frame(categories, readOnly);
        };
        frame(false, false);
        if (!click(false, false) || light.IsEnabled)
            return Fail("Checkbox input did not reach the reflected component.");
        frame(false, true);
        if (click(false, true) || light.IsEnabled)
            return Fail("A read-only component accepted widget input.");
        frame(true, false);
        const float opened = expandedHeight;
        click(true, false);
        if (expandedHeight >= opened) return Fail("Property category did not collapse.");
        auto& localization = Engine::LocalizationManager::Get();
        static_cast<void>(localization.SetCurrentLanguage(ELocalizationLanguage::English));
        frame(true, false);
        if (expandedHeight >= opened) return Fail("Language change reset the category state.");
        click(true, false);
        if (expandedHeight <= ImGui::GetFrameHeight()) return Fail("Property category did not expand.");
        const auto* hierarchyType = registry.FindType<Engine::HierarchyComponent>();
        if (!hierarchyType || Editor::ReflectionPropertyDrawer::IsVisible(*hierarchyType) ||
            !Editor::ReflectionPropertyDrawer::IsVisible(*nameType))
            return Fail("Type visibility metadata was not respected.");
        Engine::HierarchyComponent hierarchy;
        ImGui::NewFrame();
        ImGui::Begin("HiddenComponentCheck");
        const auto before = ImGui::GetCursorScreenPos();
        const auto object = reflection::ObjectView::From(hierarchy);
        bool changed = Editor::ReflectionPropertyDrawer::DrawObject(*hierarchyType, object, &registry);
        changed |= Editor::ReflectionPropertyDrawer::DrawPropertyTable(*hierarchyType, object, &registry);
        const auto after = ImGui::GetCursorScreenPos();
        ImGui::End();
        ImGui::Render();
        if (changed || before.x != after.x || before.y != after.y)
            return Fail("A hidden component still consumed Inspector space.");
        entt::registry world;
        const auto entity = world.create();
        world.emplace<Engine::HierarchyComponent>(entity);
        bool visitedHierarchy = false;
        const auto result = Engine::ReflectionRegistry::Get().ForEachComponent(world, entity,
            [&](const reflection::TypeInfo&, const reflection::ObjectView& value)
            {
                visitedHierarchy |= value.Is<Engine::HierarchyComponent>();
                return true;
            });
        if (result != Engine::ComponentVisitResult::Completed || !visitedHierarchy)
            return Fail("Inspector visibility incorrectly removed the component from engine traversal.");
        return true;
    }

    bool CheckCommonBoolDrawers(const reflection::Registry& registry)
    {
        struct Owner
        {
            bool Enabled = false;
            bool Gate = true;
            int Keep = 0;
        };

        Owner first{ false, true, 11 }, second{ true, true, 22 };
        auto type = reflection::TypeInfo::For<Owner>("CommonBoolOwner");
        auto enabled = reflection::MakeProperty<&Owner::Enabled>("Enabled", "bool");
        static_cast<void>(enabled.Metadata.Add("Editable", true));
        static_cast<void>(enabled.Metadata.Add("DisplayName", std::string("PROP_ENABLED")));
        static_cast<void>(enabled.Metadata.Add("EditCondition", std::string("Gate")));
        type.Properties.push_back(std::move(enabled));

        auto gate = reflection::MakeProperty<&Owner::Gate>("Gate", "bool");
        static_cast<void>(gate.Metadata.Add("Visible", false));
        type.Properties.push_back(std::move(gate));

        Editor::ReflectionObjectEditSettings secondSettings;
        auto& io = ImGui::GetIO();
        const auto frame = [&](bool readOnly)
            {
                const std::array objects{
                    reflection::ObjectView::From(first),
                    readOnly ? reflection::ObjectView::From(std::as_const(second)) :
                        reflection::ObjectView::From(second)
                };
                const std::array<const Editor::ReflectionObjectEditSettings*, 2> settings{ nullptr, &secondSettings };

                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2(600, 500));
                ImGui::Begin("CommonBoolCheck");
                ImGui::SetCursorScreenPos(ImVec2(20, 40));
                const bool changed = Editor::ReflectionPropertyDrawer::DrawCommonBoolProperties(
                    type, objects, &registry, settings);
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
                ImGui::End();
                ImGui::Render();
                return changed;
            };
        const auto click = [&](bool readOnly)
            {
                io.AddMousePosEvent(28, 48);
                frame(readOnly);
                io.AddMouseButtonEvent(0, true);
                frame(readOnly);
                io.AddMouseButtonEvent(0, false);
                return frame(readOnly);
            };

        if (frame(false) || first.Enabled || !second.Enabled)
            return Fail("Drawing common bools changed data without input.");
        if (!click(false) || !first.Enabled || !second.Enabled || first.Keep != 11 || second.Keep != 22)
            return Fail("Mixed bool editing failed or changed another member.");
        if (!click(false) || first.Enabled || second.Enabled)
            return Fail("Common bool editing did not disable every target.");
        if (click(true) || first.Enabled || second.Enabled)
            return Fail("A const target did not block the common edit.");

        secondSettings.ReadOnlyProperties.insert("Enabled");
        if (click(false) || first.Enabled || second.Enabled)
            return Fail("A property lock did not block the common edit.");
        secondSettings.ReadOnlyProperties.clear();

        first.Gate = false;
        if (click(false) || first.Enabled || second.Enabled)
            return Fail("A target failing EditCondition accepted the common edit.");
        first.Gate = true;

        static_cast<void>(type.Properties.front().Metadata.Add("ReadOnly", true));
        if (click(false) || first.Enabled || second.Enabled)
            return Fail("ReadOnly metadata did not block the common edit.");
        return true;
    }

    bool CheckCommonValueWrites()
    {
        struct Owner
        {
            bool Enabled = false;
            float Mass = 0.0f;
            int Keep = 0;
        };

        Owner first{ false, 1.0f, 11 }, second{ true, 2.0f, 22 };
        const std::array objects{
            reflection::ObjectView::From(first), reflection::ObjectView::From(second)
        };
        const auto mass = reflection::MakeProperty<&Owner::Mass>("Mass", "float");
        const auto enabled = reflection::MakeProperty<&Owner::Enabled>("Enabled", "bool");

        auto result = Editor::TryWriteCommonInspectorValue(mass, objects, 3.5f);
        if (result.Failed || !result.Changed || first.Mass != 3.5f || second.Mass != 3.5f ||
            first.Enabled || !second.Enabled || first.Keep != 11 || second.Keep != 22)
            return Fail("Common float writing failed or changed another member.");

        result = Editor::TryWriteCommonInspectorValue(mass, objects, 3.5f);
        if (result.Failed || result.Changed)
            return Fail("Writing an unchanged common value reported a change.");

        result = Editor::TryWriteCommonInspectorValue(enabled, objects, true);
        if (result.Failed || !result.Changed || !first.Enabled || !second.Enabled)
            return Fail("Common bool writing failed.");

        const std::array readOnlyObjects{
            reflection::ObjectView::From(first),
            reflection::ObjectView::From(std::as_const(second))
        };
        result = Editor::TryWriteCommonInspectorValue(mass, readOnlyObjects, 6.0f);
        if (!result.Failed || result.Changed || first.Mass != 3.5f || second.Mass != 3.5f)
            return Fail("A const target allowed a partial common write.");

        struct OtherOwner
        {
            float Mass = 7.0f;
        };

        OtherOwner other;
        const std::array mismatchedObjects{
            reflection::ObjectView::From(first), reflection::ObjectView::From(other)
        };
        result = Editor::TryWriteCommonInspectorValue(mass, mismatchedObjects, 6.0f);
        if (!result.Failed || result.Changed || first.Mass != 3.5f || other.Mass != 7.0f)
            return Fail("An owner type mismatch allowed a partial common write.");

        result = Editor::TryWriteCommonInspectorValue(mass, objects, true);
        if (!result.Failed || result.Changed || first.Mass != 3.5f || second.Mass != 3.5f)
            return Fail("A mismatched value type was accepted.");

        const std::array invalidValues{
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()
        };
        for (const float candidate : invalidValues)
        {
            result = Editor::TryWriteCommonInspectorValue(mass, objects, candidate);
            if (!result.Failed || result.Changed || first.Mass != 3.5f || second.Mass != 3.5f)
                return Fail("A non-finite common float value was accepted.");
        }

        result = Editor::TryWriteCommonInspectorValue(mass, std::span<const reflection::ObjectView>{}, 6.0f);
        if (!result.Failed || result.Changed)
            return Fail("An empty common write was accepted.");

        return true;
    }

    bool CheckCommonFloatDrawers(const reflection::Registry& registry)
    {
        struct Owner
        {
            float Amount = 0.0f;
            bool Gate = true;
            int Keep = 0;
        };

        Owner first{ 1.0f, true, 11 }, second{ 2.0f, true, 22 };
        auto type = reflection::TypeInfo::For<Owner>("CommonFloatOwner");
        auto amount = reflection::MakeProperty<&Owner::Amount>("Amount", "float");
        static_cast<void>(amount.Metadata.Add("Editable", true));
        static_cast<void>(amount.Metadata.Add("RangeMin", 0.0));
        static_cast<void>(amount.Metadata.Add("RangeMax", 10.0));
        static_cast<void>(amount.Metadata.Add("EditCondition", std::string("Gate")));
        type.Properties.push_back(std::move(amount));

        auto gate = reflection::MakeProperty<&Owner::Gate>("Gate", "bool");
        static_cast<void>(gate.Metadata.Add("Visible", false));
        type.Properties.push_back(std::move(gate));

        Editor::ReflectionObjectEditSettings secondSettings;
        auto& io = ImGui::GetIO();
        bool premature = false;

        const auto frame = [&](bool readOnly)
            {
                const std::array objects{
                    reflection::ObjectView::From(first),
                    readOnly ? reflection::ObjectView::From(std::as_const(second)) :
                        reflection::ObjectView::From(second)
                };
                const std::array<const Editor::ReflectionObjectEditSettings*, 2> settings{ nullptr, &secondSettings };

                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2(600, 500));
                ImGui::Begin("CommonFloatCheck");
                ImGui::SetCursorScreenPos(ImVec2(20, 40));
                const bool changed = Editor::ReflectionPropertyDrawer::DrawCommonFloatProperties(
                    type, objects, &registry, settings);
                ImGui::Dummy(ImVec2(0, 0));
                ImGui::End();
                ImGui::Render();
                return changed;
            };

        const auto submit = [&](const char* text, bool readOnly = false)
            {
                const float previousFirst = first.Amount;
                const float previousSecond = second.Amount;

                io.AddMousePosEvent(28, 48);
                premature |= frame(readOnly);
                io.AddMouseButtonEvent(0, true);
                premature |= frame(readOnly);
                io.AddMouseButtonEvent(0, false);
                premature |= frame(readOnly);

                io.AddInputCharactersUTF8(text);
                premature |= frame(readOnly);
                premature |= first.Amount != previousFirst || second.Amount != previousSecond;

                io.AddKeyEvent(ImGuiKey_Enter, true);
                const bool changed = frame(readOnly);
                io.AddKeyEvent(ImGuiKey_Enter, false);
                premature |= frame(readOnly);
                return changed;
            };

        if (frame(false) || first.Amount != 1.0f || second.Amount != 2.0f)
            return Fail("Drawing common floats changed data without input.");
        if (!submit("3.5") || first.Amount != 3.5f || second.Amount != 3.5f ||
            first.Keep != 11 || second.Keep != 22)
            return Fail("Common float input failed or changed another member.");
        if (!submit("99") || first.Amount != 10.0f || second.Amount != 10.0f)
            return Fail("The common float maximum was not enforced.");
        if (!submit("-5") || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("The common float minimum was not enforced.");
        if (submit("1e99") || submit("1e") || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("Invalid common float input was accepted.");
        if (submit("6", true) || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("A const target accepted common float input.");

        secondSettings.ReadOnlyProperties.insert("Amount");
        if (submit("6") || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("A property lock accepted common float input.");
        secondSettings.ReadOnlyProperties.clear();

        secondSettings.Vectors["Amount"].Locked = true;
        if (submit("6") || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("An edit lock accepted common float input.");
        secondSettings.Vectors.clear();

        first.Gate = false;
        if (submit("6") || first.Amount != 0.0f || second.Amount != 0.0f)
            return Fail("A failed EditCondition accepted common float input.");

        if (premature) return Fail("Common float input changed data before Enter or without a new request.");
        return true;
    }

    bool CheckCommonEnumWrites()
    {
        enum class Mode : std::int8_t
        {
            Idle = -1,
            Running = 2
        };

        struct Owner
        {
            Mode Value = Mode::Idle;
            int Keep = 0;
        };

        auto enumeration = reflection::EnumInfo::For<Mode>("CommonEnumMode");
        enumeration.Entries.push_back({ "Idle", reflection::EnumValue{ std::int64_t{ -1 } } });
        enumeration.Entries.push_back({ "Running", reflection::EnumValue{ std::int64_t{ 2 } } });

        const auto property = reflection::MakeProperty<&Owner::Value>("Value", "Mode");
        Owner first{ Mode::Idle, 11 }, second{ Mode::Running, 22 };
        const std::array objects{
            reflection::ObjectView::From(first), reflection::ObjectView::From(second)
        };

        const reflection::EnumValue running{ std::uint64_t{ 2 } };
        auto result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, objects, running);
        if (result.Failed || !result.Changed || first.Value != Mode::Running || second.Value != Mode::Running ||
            first.Keep != 11 || second.Keep != 22)
            return Fail("Common enum writing failed or changed another member.");

        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, objects, running);
        if (result.Failed || result.Changed)
            return Fail("Equivalent signed and unsigned enum values reported a change.");

        const reflection::EnumValue idle{ std::int64_t{ -1 } };
        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, objects, idle);
        if (result.Failed || !result.Changed || first.Value != Mode::Idle || second.Value != Mode::Idle)
            return Fail("A negative common enum value was not preserved.");

        const reflection::EnumValue unknown{ std::int64_t{ 3 } };
        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, objects, unknown);
        if (!result.Failed || result.Changed || first.Value != Mode::Idle || second.Value != Mode::Idle)
            return Fail("An unregistered enum value was accepted.");

        const std::array readOnlyObjects{
            reflection::ObjectView::From(first),
            reflection::ObjectView::From(std::as_const(second))
        };
        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, readOnlyObjects, running);
        if (!result.Failed || result.Changed || first.Value != Mode::Idle || second.Value != Mode::Idle)
            return Fail("A const target allowed a partial enum write.");

        struct OtherOwner
        {
            Mode Value = Mode::Idle;
        };

        OtherOwner other;
        const std::array mismatchedObjects{
            reflection::ObjectView::From(first), reflection::ObjectView::From(other)
        };
        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, mismatchedObjects, running);
        if (!result.Failed || result.Changed || first.Value != Mode::Idle || other.Value != Mode::Idle)
            return Fail("An owner type mismatch allowed a partial enum write.");

        // 잘못 등록된 값도 실제 enum 저장 범위를 넘어 쓰지 못해야 합니다.
        const reflection::EnumValue outOfRange{ std::int64_t{ 256 } };
        enumeration.Entries.push_back({ "OutOfRange", outOfRange });
        result = Editor::TryWriteCommonInspectorEnumValue(property, enumeration, objects, outOfRange);
        if (!result.Failed || result.Changed || first.Value != Mode::Idle || second.Value != Mode::Idle)
            return Fail("An enum value outside its underlying range was accepted.");

        enum class WideMode : std::uint64_t
        {
            Low = 0,
            High = (std::numeric_limits<std::uint64_t>::max)()
        };

        struct WideOwner
        {
            WideMode Value = WideMode::Low;
        };

        auto wideEnum = reflection::EnumInfo::For<WideMode>("CommonWideEnum");
        const reflection::EnumValue high{ static_cast<std::uint64_t>(WideMode::High) };
        wideEnum.Entries.push_back({ "Low", reflection::EnumValue{ std::uint64_t{ 0 } } });
        wideEnum.Entries.push_back({ "High", high });

        const auto wideProperty = reflection::MakeProperty<&WideOwner::Value>("Value", "WideMode");
        WideOwner wideFirst, wideSecond{ WideMode::High };
        const std::array wideObjects{
            reflection::ObjectView::From(wideFirst), reflection::ObjectView::From(wideSecond)
        };

        result = Editor::TryWriteCommonInspectorEnumValue(wideProperty, wideEnum, wideObjects, high);
        if (result.Failed || !result.Changed ||
            wideFirst.Value != WideMode::High || wideSecond.Value != WideMode::High)
            return Fail("A large unsigned enum value was corrupted.");

        return true;
    }

    bool CheckDrawers()
    {
        const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
        const auto* rigid = registry.FindType<Engine::RigidBodyComponent>();
        const auto* collider = registry.FindType<Engine::Collider2DComponent>();
        const auto* joint = registry.FindType<Engine::Joint2DComponent>();
        if (!rigid || rigid->Properties.empty() || !collider || !joint ||
            !registry.FindEnum<Engine::ERigidBodyType>())
            return Fail("Physics component properties or enums are missing.");
        const auto* binding = Engine::ReflectionRegistry::Get().FindComponent(
            std::type_index(typeid(Engine::CameraComponent)));
        if (!binding || binding->Name != "STRUCT_CAMERA" || binding->Category != "CATEGORY_RENDER")
            return Fail("AddComponent bindings do not use generated localization keys.");
        auto& localization = Engine::LocalizationManager::Get();
        if (localization.RegisterLocalizationData(L"Resources/Data/Localization/English.json") != EResult::Success ||
            localization.RegisterLocalizationData(L"Resources/Data/Localization/Korean.json") != EResult::Success ||
            localization.LoadData() != EResult::Success)
            return Fail("Generated localization catalogs could not be loaded.");
        for (const auto language : { ELocalizationLanguage::English, ELocalizationLanguage::Korean })
        {
            static_cast<void>(localization.SetCurrentLanguage(language));
            for (const auto* key : { "CATEGORY_CORE", "CATEGORY_RENDER", "CATEGORY_PHYSICS_2D",
                "CATEGORY_PHYSICS_3D", "STRUCT_CAMERA", "STRUCT_COLLIDER", "UI_GENERAL", "UI_RESOURCE_NONE",
                "UI_RESOURCE_CLEAR", "UI_RESOURCE_REJECTED", "UI_INVALID_PROPERTY", "UI_ADD_VALUE" })
            {
                const auto text = localization.GetText(std::string(key));
                if (text.empty() || text == key) return Fail("An Inspector label is missing from generated catalogs.");
            }
        }
        auto* context = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(1600.0f, 1200.0f);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Engine::LightComponent light;
        Engine::SpriteRendererComponent sprite;
        Engine::ColliderComponent shape3D;
        Engine::Collider2DComponent shape2D;
        Engine::NameComponent name;
        Engine::TagComponent tags;
        Engine::RigidBodyComponent rigidBody;
        Editor::ReflectionObjectEditSettings settings;
        for (const auto language : { ELocalizationLanguage::English, ELocalizationLanguage::Korean })
        {
            static_cast<void>(localization.SetCurrentLanguage(language));
            if (Editor::LocalizeInspectorText("CATEGORY_RENDER") !=
                (language == ELocalizationLanguage::English ? "Rendering" : "렌더링"))
                return Fail("Inspector localization did not react to a language change.");
            int drawIndex = 0;
            const auto draw = [&](auto& value)
            {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(1550.0f, 1150.0f));
                ImGui::Begin("InspectorCheck");
                ImGui::PushID(drawIndex++);
                const auto* type = registry.FindType<std::remove_cvref_t<decltype(value)>>();
                const bool changed = type && Editor::ReflectionPropertyDrawer::DrawObject(
                    *type, reflection::ObjectView::From(value), &registry, &settings);
                ImGui::PopID();
                ImGui::End();
                ImGui::Render();
                return changed;
            };
            if (draw(light) || draw(sprite) || draw(shape3D) || draw(shape2D) || draw(name) ||
                draw(tags) || draw(rigidBody) || draw(std::as_const(sprite)))
                return Fail("Drawing controls without input changed component data.");
        }
        if (!CheckInteractions(registry) || !CheckDragDrop(registry) ||
            !CheckCommonBoolDrawers(registry) || !CheckCommonFloatDrawers(registry))
            return false;
        ImGui::DestroyContext(context);
        return true;
    }
}

int main()
{
    Scope scope;
    if (!Engine::LocalizationManager::Create() || !Engine::ResourceManager::Create() ||
        !Engine::ReflectionRegistry::Create() || !Engine::InputManager::Create()) return 1;
    if (!CheckPayloads() || !CheckResourceTypes() || !CheckColliderValidation() ||
        !CheckJointArchiveBoundary() || !CheckCommonValueWrites() || !CheckCommonEnumWrites() ||
        !CheckDrawers()) return 1;
    std::cout << "Inspector resource, reflection, validation, and draw checks passed.\n";
    return 0;
}

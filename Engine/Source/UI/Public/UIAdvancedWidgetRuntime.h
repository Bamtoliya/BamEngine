#pragma once

#include "CanvasComponent.h"
#include "CanvasScalerComponent.h"
#include "UITextInputComponent.h"
#include "UIDropdownComponent.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

namespace Engine
{
    enum class EUIKey
    {
        Unknown,
        Backspace,
        Delete,
        Left,
        Right,
        Home,
        End,
        Enter,
        Escape,
        Tab,
        A,
        C,
        V,
        X
    };

    struct UIKeyModifiers
    {
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
    };

    inline bool HandleTextInputKey(
        UITextInputComponent& input,
        EUIKey key,
        UIKeyModifiers modifiers = {},
        std::string* outClipboardText = nullptr,
        std::string_view pasteText = {})
    {
        if (!input.isFocused) return false;

        if (modifiers.ctrl)
        {
            if (key == EUIKey::A)
            {
                input.SelectAll();
                return true;
            }
            if (key == EUIKey::C)
            {
                if (outClipboardText && input.HasSelection())
                {
                    *outClipboardText = input.GetSelectedText();
                }
                return true;
            }
            if (key == EUIKey::X)
            {
                if (!input.readOnly && input.HasSelection())
                {
                    if (outClipboardText)
                    {
                        *outClipboardText = input.GetSelectedText();
                    }
                    input.DeleteSelection();
                    return true;
                }
                return false;
            }
            if (key == EUIKey::V)
            {
                if (!input.readOnly && !pasteText.empty())
                {
                    return input.InsertUtf8(pasteText);
                }
                return false;
            }
        }

        switch (key)
        {
        case EUIKey::Backspace:
            return input.Backspace();

        case EUIKey::Delete:
            return input.DeleteForward();

        case EUIKey::Left:
            input.MoveCursorLeft(modifiers.shift);
            return true;

        case EUIKey::Right:
            input.MoveCursorRight(modifiers.shift);
            return true;

        case EUIKey::Home:
            if (modifiers.shift)
            {
                input.cursorByteIndex = 0;
            }
            else
            {
                input.cursorByteIndex = 0;
                input.selectionAnchorByteIndex = 0;
            }
            return true;

        case EUIKey::End:
            if (modifiers.shift)
            {
                input.cursorByteIndex = input.text.size();
            }
            else
            {
                input.cursorByteIndex = input.text.size();
                input.selectionAnchorByteIndex = input.text.size();
            }
            return true;

        case EUIKey::Enter:
            if (input.multiline && !input.readOnly)
            {
                return input.InsertUtf8("\n");
            }
            else
            {
                input.isFocused = false;
                return true;
            }

        case EUIKey::Escape:
            input.CancelImeComposition();
            input.ClearSelection();
            input.isFocused = false;
            return true;

        default:
            return false;
        }
    }

    inline bool HandleTextInputCommit(UITextInputComponent& input, std::string_view utf8Text)
    {
        if (!input.isFocused || input.readOnly || utf8Text.empty()) return false;
        input.CancelImeComposition();
        return input.InsertUtf8(utf8Text);
    }

    inline bool HandleTextInputEditing(UITextInputComponent& input, std::string_view compositionText)
    {
        if (!input.isFocused || input.readOnly) return false;
        input.SetImeComposition(compositionText);
        return true;
    }

    inline void SetTextInputFocus(entt::registry& world, entt::entity targetEntity)
    {
        auto view = world.view<UITextInputComponent>();
        for (auto entity : view)
        {
            auto& input = view.get<UITextInputComponent>(entity);
            const bool shouldFocus = (entity == targetEntity);
            if (input.isFocused != shouldFocus)
            {
                input.isFocused = shouldFocus;
                if (!shouldFocus)
                {
                    input.CancelImeComposition();
                    input.ClearSelection();
                }
            }
        }
    }

    inline bool HandleDropdownClick(
        UIDropdownComponent& dropdown,
        const glm::vec2& clickPoint,
        const glm::vec2& headerPos,
        const glm::vec2& headerSize)
    {
        if (!dropdown.interactable) return false;

        const bool inHeader = (clickPoint.x >= headerPos.x &&
                               clickPoint.x <= headerPos.x + headerSize.x &&
                               clickPoint.y >= headerPos.y &&
                               clickPoint.y <= headerPos.y + headerSize.y);

        if (inHeader)
        {
            dropdown.ToggleOpen();
            return true;
        }

        if (dropdown.isOpen)
        {
            const float popupHeight = dropdown.GetPopupHeight();
            const glm::vec2 popupPos{ headerPos.x, headerPos.y + headerSize.y };
            const bool inPopup = (clickPoint.x >= popupPos.x &&
                                  clickPoint.x <= popupPos.x + headerSize.x &&
                                  clickPoint.y >= popupPos.y &&
                                  clickPoint.y <= popupPos.y + popupHeight);

            if (inPopup)
            {
                const float localY = clickPoint.y - popupPos.y;
                const int hitIndex = dropdown.HitTestOptionIndex(localY);
                if (hitIndex >= 0)
                {
                    dropdown.SelectIndex(hitIndex);
                    return true;
                }
            }
            else
            {
                dropdown.Close();
                return false;
            }
        }

        return false;
    }

    inline bool HandleDropdownClick(
        entt::registry& world,
        entt::entity dropdownEntity,
        const glm::vec2& clickPoint,
        const glm::vec2& headerPos,
        const glm::vec2& headerSize)
    {
        auto* dropdown = world.try_get<UIDropdownComponent>(dropdownEntity);
        if (!dropdown) return false;
        return HandleDropdownClick(*dropdown, clickPoint, headerPos, headerSize);
    }

    inline void CloseAllDropdowns(entt::registry& world, entt::entity exceptEntity = entt::null)
    {
        auto view = world.view<UIDropdownComponent>();
        for (auto entity : view)
        {
            if (entity != exceptEntity)
            {
                view.get<UIDropdownComponent>(entity).Close();
            }
        }
    }

    inline bool UpdateCanvasScaler(entt::registry& world, entt::entity canvasEntity, const glm::vec2& viewportSize)
    {
        auto* scaler = world.try_get<CanvasScalerComponent>(canvasEntity);
        auto* canvas = world.try_get<CanvasComponent>(canvasEntity);
        if (!scaler || !canvas) return false;

        canvas->referenceResolution = scaler->ComputeEffectiveDesignSize(viewportSize);
        return true;
    }
}

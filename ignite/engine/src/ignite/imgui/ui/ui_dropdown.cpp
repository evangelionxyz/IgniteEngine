// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_dropdown.hpp"
#include "ui_input.hpp"
#include "icons_lucide.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <unordered_map>

namespace ignite::UI
{
    namespace
    {
        std::unordered_map<ImGuiID, std::string> &GetDropdownSearchMap()
        {
            static std::unordered_map<ImGuiID, std::string> s_Search;
            return s_Search;
        }

        bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
                return true;
            if (haystack.size() < needle.size())
                return false;

            auto it = std::search(
                haystack.begin(), haystack.end(),
                needle.begin(), needle.end(),
                [](char a, char b)
                {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != haystack.end();
        }

        void PushPopoverStyle()
        {
            const ThemeColors &t = GetTheme();
            const float scale = std::max(GetDpiScale(), 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ToImVec4(t.popover));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ToImVec4(t.popover));
            ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(t.border));
            ImGui::PushStyleColor(ImGuiCol_Header, ToImVec4(t.secondaryHover));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToImVec4(t.secondaryHover));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ToImVec4(t.secondaryActive));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * scale, 6.0f * scale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * scale, 2.0f * scale));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        }

        void PopPopoverStyle()
        {
            ImGui::PopStyleVar(6);
            ImGui::PopStyleColor(6);
        }

        bool IsRootOfOpenMenuSet()
        {
            ImGuiContext &g = *GImGui;
            ImGuiWindow *window = g.CurrentWindow;
            if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & ImGuiWindowFlags_ChildMenu))
                return false;

            const ImGuiPopupData *upperPopup = &g.OpenPopupStack[g.BeginPopupStack.Size];
            if (window->DC.NavLayerCurrent != upperPopup->ParentNavLayer)
                return false;
            return upperPopup->Window && (upperPopup->Window->Flags & ImGuiWindowFlags_ChildMenu) && ImGui::IsWindowChildOf(upperPopup->Window, window, true, false);
        }
    }

    bool Dropdown(
        const char *id,
        const char *const *items,
        int itemsCount,
        int *selectedIndex,
        const char *placeholder,
        float width,
        bool searchable,
        float height)
    {
        if (!selectedIndex)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (width > 0.0f) ? width : std::max(80.0f, ImGui::GetContentRegionAvail().x);
        const float h = (height > 0.0f) ? height : std::round(m.inputHeight * scale);
        const float padX = std::round(8.0f * scale);

        ImGui::PushID(id ? id : "##dropdown");
        const ImGuiID popupId = ImGui::GetID("##dd_popup");
        const bool isOpen = ImGui::IsPopupOpen(popupId, ImGuiPopupFlags_None);

        const ImVec2 triggerMin = ImGui::GetCursorScreenPos();
        const ImVec2 triggerMax(triggerMin.x + w, triggerMin.y + h);
        const bool clicked = ImGui::InvisibleButton("##dd_trigger", ImVec2(w, h));
        const bool isHovered = ImGui::IsItemHovered();

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (clicked)
        {
            ImGui::OpenPopup(popupId);
            if (searchable)
                GetDropdownSearchMap()[popupId].clear();
        }

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(triggerMin, triggerMax, ToU32(isHovered || isOpen ? t.inputBgHover : t.inputBg), 0.0f);
        if (isOpen)
        {
            dl->AddRect(triggerMin, triggerMax, ToU32(t.primary), 0.0f, 0, 1.0f);
            dl->AddRect(ImVec2(triggerMin.x - 2.0f, triggerMin.y - 2.0f), ImVec2(triggerMax.x + 2.0f, triggerMax.y + 2.0f), ToU32(t.ring, 0.22f), 0.0f, 0, 2.0f);
        }
        else
        {
            dl->AddRect(triggerMin, triggerMax, ToU32(isHovered ? t.borderStrong : t.border), 0.0f, 0, 1.0f);
        }

        const bool hasValidSelection = (items && itemsCount > 0 && *selectedIndex >= 0 && *selectedIndex < itemsCount && items[*selectedIndex] != nullptr);
        const char *displayText = hasValidSelection ? items[*selectedIndex] : (placeholder ? placeholder : "Select...");
        const ImVec2 chevronSz = ImGui::CalcTextSize(ICON_LC_CHEVRON_DOWN);
        const ImVec2 textSz = ImGui::CalcTextSize(displayText);

        const float textY = triggerMin.y + std::round((h - textSz.y) * 0.5f);
        const float chevronX = triggerMax.x - padX - chevronSz.x;
        const float chevronY = triggerMin.y + std::round((h - chevronSz.y) * 0.5f);

        const ImVec4 textClip(triggerMin.x + padX, triggerMin.y, std::max(triggerMin.x + padX + 4.0f, chevronX - 6.0f), triggerMax.y);
        dl->AddText(nullptr, 0.0f, ImVec2(triggerMin.x + padX, textY), ToU32(hasValidSelection ? t.foreground : t.mutedForeground), displayText, nullptr, 0.0f, &textClip);
        dl->AddText(ImVec2(chevronX, chevronY), ToU32(t.mutedForeground), ICON_LC_CHEVRON_DOWN);

        bool changed = false;
        ImGui::SetNextWindowPos(ImVec2(triggerMin.x, triggerMax.y + 4.0f), ImGuiCond_Always);
        const float maxPopupH = std::round(280.0f * scale);
        ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(std::max(w, 320.0f * scale), maxPopupH));

        PushPopoverStyle();
        if (ImGui::BeginPopupEx(popupId, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove))
        {
            std::string &query = GetDropdownSearchMap()[popupId];
            if (searchable)
            {
                SearchField("##dd_search", query, "Filter options...", std::max(80.0f, ImGui::GetContentRegionAvail().x));
                ImGui::Spacing();
            }

            if (!items || itemsCount <= 0)
            {
                ImGui::TextDisabled("No options");
            }
            else
            {
                const float itemH = std::round(m.menuItemHeight * scale);
                const float availW = ImGui::GetContentRegionAvail().x;
                int visibleCount = 0;

                for (int i = 0; i < itemsCount; ++i)
                {
                    const char *itemLabel = items[i] ? items[i] : "";
                    if (searchable && !query.empty() && !ContainsCaseInsensitive(itemLabel, query))
                        continue;

                    ++visibleCount;
                    ImGui::PushID(i);
                    const ImVec2 itemMin = ImGui::GetCursorScreenPos();
                    const ImVec2 itemMax(itemMin.x + availW, itemMin.y + itemH);
                    const bool itemClicked = ImGui::InvisibleButton("##dd_item", ImVec2(availW, itemH));
                    const bool itemHovered = ImGui::IsItemHovered();
                    if (itemHovered)
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                    const bool isSelected = (*selectedIndex == i);
                    ImDrawList *pdl = ImGui::GetWindowDrawList();
                    if (itemHovered)
                        pdl->AddRectFilled(itemMin, itemMax, ToU32(t.secondaryHover), 0.0f);
                    else if (isSelected)
                        pdl->AddRectFilled(itemMin, itemMax, ToU32(t.secondary), 0.0f);

                    const ImVec2 lblSz = ImGui::CalcTextSize(itemLabel);
                    pdl->AddText(ImVec2(itemMin.x + 8.0f * scale, itemMin.y + std::round((itemH - lblSz.y) * 0.5f)), ToU32(isSelected || itemHovered ? t.foreground : t.mutedForeground), itemLabel);

                    if (isSelected)
                    {
                        const ImVec2 chkSz = ImGui::CalcTextSize(ICON_LC_CHECK);
                        pdl->AddText(ImVec2(itemMax.x - 8.0f * scale - chkSz.x, itemMin.y + std::round((itemH - chkSz.y) * 0.5f)), ToU32(t.primary), ICON_LC_CHECK);
                    }

                    if (itemClicked)
                    {
                        if (*selectedIndex != i)
                        {
                            *selectedIndex = i;
                            changed = true;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }

                if (visibleCount == 0)
                {
                    {
                        ScopedFont smallFont(FontVariant::Small);
                        ImGui::TextDisabled("No results found.");
                    }
                }
            }
            ImGui::EndPopup();
        }
        PopPopoverStyle();

        ImGui::PopID();
        return changed;
    }

    bool Dropdown(
        const char *id,
        std::span<const std::string> items,
        int *selectedIndex,
        const char *placeholder,
        float width,
        bool searchable,
        float height)
    {
        std::vector<const char *> ptrs;
        ptrs.reserve(items.size());
        for (const auto &s : items)
        {
            ptrs.push_back(s.c_str());
        }
        return Dropdown(id, ptrs.data(), static_cast<int>(ptrs.size()), selectedIndex, placeholder, width, searchable, height);
    }

    bool BeginContextMenu(const char *id)
    {
        PushPopoverStyle();
        if (ImGui::BeginPopupContextItem(id))
            return true;
        PopPopoverStyle();
        return false;
    }

    bool BeginWindowContextMenu(const char *id, ImGuiPopupFlags flags)
    {
        PushPopoverStyle();
        if (ImGui::BeginPopupContextWindow(id, flags))
            return true;
        PopPopoverStyle();
        return false;
    }

    void OpenPopupMenu(const char *id)
    {
        ImGui::OpenPopup(id);
    }

    bool BeginPopupMenu(const char *id)
    {
        PushPopoverStyle();
        if (ImGui::BeginPopup(id))
            return true;
        PopPopoverStyle();
        return false;
    }

    void EndContextMenu()
    {
        ImGui::EndPopup();
        PopPopoverStyle();
    }

    bool ContextMenuItem(
        const char *label,
        const char *shortcut,
        const char *icon,
        bool destructive,
        bool enabled,
        const glm::vec4 &iconColor)
    {
        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float h = std::round(m.menuItemHeight * scale);
        const float minW = std::round(190.0f * scale);
        const float padX = std::round(8.0f * scale);
        const float iconSlotW = std::round(22.0f * scale);

        const char *safeLabel = label ? label : "##item";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const bool hasIcon = (icon && icon[0] != '\0');
        const bool hasShortcut = (shortcut && shortcut[0] != '\0');

        const ImVec2 lblSz = ImGui::CalcTextSize(safeLabel, labelEnd, false);
        ImVec2 scSz(0.0f, 0.0f);
        if (hasShortcut)
        {
            ScopedFont smallFont(FontVariant::Small);
            scSz = ImGui::CalcTextSize(shortcut);
        }

        const float neededW = padX * 2.0f
            + (hasIcon ? iconSlotW : 0.0f)
            + lblSz.x
            + (hasShortcut ? (scSz.x + std::round(20.0f * scale)) : std::round(8.0f * scale));
        const float availW = std::max({ minW, neededW, ImGui::GetContentRegionAvail().x });

        ImGuiContext &g = *GImGui;
        if (IsRootOfOpenMenuSet())
            g.NextItemData.ItemFlagsSet |= ImGuiItemFlags_NoWindowHoverableCheck;

        if (!enabled)
            ImGui::BeginDisabled(true);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton(safeLabel, ImVec2(availW, h));
        const bool hovered = enabled && ImGui::IsItemHovered();
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (hovered)
        {
            const glm::vec4 hoverBg = destructive ? WithAlpha(t.destructive, 0.35f) : t.secondaryHover;
            dl->AddRectFilled(pos, ImVec2(pos.x + availW, pos.y + h), ToU32(hoverBg), 0.0f);
        }

        glm::vec4 textCol = destructive ? t.destructiveBorder : (hovered ? t.foreground : t.foreground);
        if (!enabled)
            textCol = t.disabledForeground;

        float x = pos.x + padX;
        if (hasIcon)
        {
            const glm::vec4 resolvedIconCol = !enabled
                ? t.disabledForeground
                : (destructive
                    ? t.destructiveBorder
                    : (iconColor.a > 0.001f ? iconColor : (hovered ? t.foreground : t.mutedForeground)));
            const ImVec2 icSz = ImGui::CalcTextSize(icon);
            dl->AddText(ImVec2(x, pos.y + std::round((h - icSz.y) * 0.5f)), ToU32(resolvedIconCol), icon);
            x += iconSlotW;
        }

        dl->AddText(ImVec2(x, pos.y + std::round((h - lblSz.y) * 0.5f)), ToU32(textCol), safeLabel, labelEnd);

        if (hasShortcut)
        {
            ScopedFont smallFont(FontVariant::Small);
            dl->AddText(ImVec2(pos.x + availW - padX - scSz.x, pos.y + std::round((h - scSz.y) * 0.5f)), ToU32(t.mutedForeground), shortcut);
        }

        if (clicked && enabled)
            ImGui::CloseCurrentPopup();

        if (!enabled)
            ImGui::EndDisabled();

        return clicked && enabled;
    }

    bool ContextMenuCheckboxItem(const char *label, bool *checked, const char *shortcut)
    {
        const bool isChecked = (checked && *checked);
        const bool clicked = ContextMenuItem(label, shortcut, isChecked ? ICON_LC_CHECK : " ", false, true);
        if (clicked && checked)
        {
            *checked = !(*checked);
        }
        return clicked;
    }

    bool ContextMenuBeginSubMenu(const char *label, const char *icon, bool enabled, const glm::vec4 &iconColor)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        ImGuiContext &g = *GImGui;
        const char *safeLabel = (label && label[0] != '\0') ? label : "##submenu";
        const ImGuiID id = window->GetID(safeLabel);
        bool menuIsOpen = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu)
            windowFlags |= ImGuiWindowFlags_ChildWindow;

        if (g.MenusIdSubmittedThisFrame.contains(id))
        {
            if (menuIsOpen)
            {
                PushPopoverStyle();
                menuIsOpen = ImGui::BeginPopupMenuEx(id, safeLabel, windowFlags);
                if (!menuIsOpen)
                    PopPopoverStyle();
            }
            else
            {
                g.NextWindowData.ClearFlags();
            }
            return menuIsOpen;
        }

        g.MenusIdSubmittedThisFrame.push_back(id);

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float h = std::round(m.menuItemHeight * scale);
        const float minW = std::round(190.0f * scale);
        const float padX = std::round(8.0f * scale);
        const float iconSlotW = std::round(22.0f * scale);

        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const bool hasIcon = (icon && icon[0] != '\0');
        const ImVec2 lblSz = ImGui::CalcTextSize(safeLabel, labelEnd, false);
        const ImVec2 chevSz = ImGui::CalcTextSize(ICON_LC_CHEVRON_RIGHT);
        const float neededW = padX * 2.0f
            + (hasIcon ? iconSlotW : 0.0f)
            + lblSz.x
            + chevSz.x
            + std::round(20.0f * scale);
        const float availW = std::max({ minW, neededW, ImGui::GetContentRegionAvail().x });

        const bool menusetIsOpen = IsRootOfOpenMenuSet();
        if (menusetIsOpen)
            g.NextItemData.ItemFlagsSet |= ImGuiItemFlags_NoWindowHoverableCheck;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 popupPos(pos.x, pos.y - g.Style.WindowPadding.y);

        if (!enabled)
            ImGui::BeginDisabled(true);

        const bool pressed = ImGui::InvisibleButton(safeLabel, ImVec2(availW, h), ImGuiButtonFlags_PressedOnClick);

        if (!enabled)
            ImGui::EndDisabled();

        if (g.ActiveId == id && g.HoveredId != id && g.ActiveIdSource == ImGuiInputSource_Mouse && ImGui::IsMouseDragging(0))
        {
            ImGui::ClearActiveID();
            ImGui::SetKeyOwner(ImGuiKey_MouseLeft, ImGuiKeyOwner_NoOwner);
        }

        const bool hovered = (g.HoveredId == id) && enabled && !g.NavHighlightItemUnderNav;
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImDrawList *dl = window->DrawList;
        if (enabled && (hovered || menuIsOpen))
            dl->AddRectFilled(pos, ImVec2(pos.x + availW, pos.y + h), ToU32(t.secondaryHover), 0.0f);

        float x = pos.x + padX;
        if (hasIcon)
        {
            const glm::vec4 resolvedIconCol = !enabled
                ? t.disabledForeground
                : (iconColor.a > 0.001f ? iconColor : ((hovered || menuIsOpen) ? t.foreground : t.mutedForeground));
            const ImVec2 icSz = ImGui::CalcTextSize(icon);
            dl->AddText(ImVec2(x, pos.y + std::round((h - icSz.y) * 0.5f)), ToU32(resolvedIconCol), icon);
            x += iconSlotW;
        }

        const glm::vec4 textCol = !enabled ? t.disabledForeground : t.foreground;
        dl->AddText(ImVec2(x, pos.y + std::round((h - lblSz.y) * 0.5f)), ToU32(textCol), safeLabel, labelEnd);

        const glm::vec4 chevCol = !enabled ? t.disabledForeground : ((hovered || menuIsOpen) ? t.foreground : t.mutedForeground);
        dl->AddText(ImVec2(pos.x + availW - padX - chevSz.x, pos.y + std::round((h - chevSz.y) * 0.5f)), ToU32(chevCol), ICON_LC_CHEVRON_RIGHT);

        bool wantOpen = false;
        bool wantOpenNavInit = false;
        bool wantClose = false;

        bool movingTowardChildMenu = false;
        ImGuiPopupData *childPopup = (g.BeginPopupStack.Size < g.OpenPopupStack.Size) ? &g.OpenPopupStack[g.BeginPopupStack.Size] : nullptr;
        ImGuiWindow *childMenuWindow = (childPopup && childPopup->Window && childPopup->Window->ParentWindow == window) ? childPopup->Window : nullptr;
        if (g.HoveredWindow == window && childMenuWindow != nullptr)
        {
            const float refUnit = g.FontSize;
            const float childDir = (window->Pos.x < childMenuWindow->Pos.x) ? 1.0f : -1.0f;
            const ImRect nextWindowRect = childMenuWindow->Rect();
            ImVec2 ta(g.IO.MousePos.x - g.IO.MouseDelta.x, g.IO.MousePos.y - g.IO.MouseDelta.y);
            ImVec2 tb = (childDir > 0.0f) ? nextWindowRect.GetTL() : nextWindowRect.GetTR();
            ImVec2 tc = (childDir > 0.0f) ? nextWindowRect.GetBL() : nextWindowRect.GetBR();
            const float padFarmostH = ImClamp(ImFabs(ta.x - tb.x) * 0.30f, refUnit * 0.5f, refUnit * 2.5f);
            ta.x += childDir * -0.5f;
            tb.x += childDir * refUnit;
            tc.x += childDir * refUnit;
            tb.y = ta.y + ImMax((tb.y - padFarmostH) - ta.y, -refUnit * 8.0f);
            tc.y = ta.y + ImMin((tc.y + padFarmostH) - ta.y, +refUnit * 8.0f);
            movingTowardChildMenu = ImTriangleContainsPoint(ta, tb, tc, g.IO.MousePos);
        }

        if (menuIsOpen && !hovered && g.HoveredWindow == window && !movingTowardChildMenu && !g.NavHighlightItemUnderNav && g.ActiveId == 0)
            wantClose = true;

        if (!menuIsOpen && pressed)
            wantOpen = true;
        else if (!menuIsOpen && hovered && !movingTowardChildMenu)
            wantOpen = true;
        else if (!menuIsOpen && hovered && g.HoveredIdTimer >= 0.30f && g.MouseStationaryTimer >= 0.30f)
            wantOpen = true;

        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right)
        {
            wantOpen = wantOpenNavInit = true;
            ImGui::NavMoveRequestCancel();
            ImGui::SetNavCursorVisibleAfterMove();
        }

        if (!enabled)
            wantClose = true;
        if (wantClose && ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
            ImGui::ClosePopupToLevel(g.BeginPopupStack.Size, true);

        if (g.ActiveId == id && wantOpen)
            g.ActiveIdNoClearOnFocusLoss = true;

        if (wantOpen && !menuIsOpen && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
        {
            ImGui::OpenPopup(safeLabel);
        }
        else if (wantOpen)
        {
            menuIsOpen = true;
            ImGui::OpenPopup(safeLabel, ImGuiPopupFlags_NoReopen);
        }

        if (menuIsOpen)
        {
            ImGuiLastItemData lastItemInParent = g.LastItemData;
            ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
            PushPopoverStyle();
            menuIsOpen = ImGui::BeginPopupMenuEx(id, safeLabel, windowFlags);
            if (menuIsOpen)
            {
                if (wantOpen && wantOpenNavInit && !g.NavInitRequest)
                {
                    ImGui::FocusWindow(g.CurrentWindow, ImGuiFocusRequestFlags_UnlessBelowModal);
                    ImGui::NavInitWindow(g.CurrentWindow, false);
                }

                g.LastItemData = lastItemInParent;
                if (g.HoveredWindow == window)
                    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
            else
            {
                PopPopoverStyle();
            }
        }
        else
        {
            g.NextWindowData.ClearFlags();
        }

        return menuIsOpen;
    }

    void ContextMenuEndSubMenu()
    {
        ImGui::EndMenu();
        PopPopoverStyle();
    }

    void ContextMenuLabel(const char *text)
    {
        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 sz(0.0f, 0.0f);
        {
            ScopedFont smallFont(FontVariant::Small);
            sz = ImGui::CalcTextSize(text ? text : "");
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 8.0f * scale, pos.y + 3.0f * scale), ToU32(t.mutedForeground), text ? text : "");
        }
        ImGui::Dummy(ImVec2(sz.x + 16.0f * scale, sz.y + 6.0f * scale));
    }

    void ContextMenuSeparator()
    {
        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float availW = std::max(40.0f, ImGui::GetContentRegionAvail().x);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float y = std::round(pos.y + 3.0f * scale);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + availW, y), ToU32(t.border), 1.0f);
        ImGui::Dummy(ImVec2(availW, std::round(6.0f * scale)));
    }
}


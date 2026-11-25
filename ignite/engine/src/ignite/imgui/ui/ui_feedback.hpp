// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_FEEDBACK_HPP
#define IGN_UI_FEEDBACK_HPP

#include "ui_button.hpp"
#include "ui_theme.hpp"

namespace ignite::UI
{
    IGN_API void LoadingSpinner(const char *id, float radius = 9.0f, float thickness = 2.0f, ImU32 color = 0);
    IGN_API void LoadingSpinnerTicks(const char *id, float radius = 9.0f, int spokeCount = 10);

    IGN_API void ProgressBar(float fraction, const ImVec2 &size = ImVec2(-1.0f, 8.0f), const char *overlayLabel = nullptr, bool indeterminate = false);
    IGN_API void SkeletonBox(const ImVec2 &size);

    IGN_API void Badge(const char *text, ButtonVariant variant = ButtonVariant::Secondary, const char *leadingIcon = nullptr);

    IGN_API bool BeginCard(const char *id, const ImVec2 &size = ImVec2(0.0f, 0.0f), bool dotGridBackdrop = false);
    IGN_API void EndCard();
    IGN_API void CardHeader(const char *title, const char *description = nullptr);

    IGN_API void DrawCenteredText(const char *text, uint32_t color = 0xFFFAFAFA);
    IGN_API void DrawBannerText(const char *text, const ImVec2 &size);
}

#endif


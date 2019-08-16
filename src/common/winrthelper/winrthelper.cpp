/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "winrthelper.h"

#include <wrl.h>
#include <Windows.UI.ViewManagement.h>

#include <algorithm>

namespace {

ABI::Windows::UI::Color GetThemeColor(ABI::Windows::UI::ViewManagement::UIColorType type, bool *ok)
{
    *ok = false;
    ABI::Windows::UI::Color color;
    Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings3> settings;
    if (SUCCEEDED(Windows::Foundation::ActivateInstance(Microsoft::WRL::Wrappers::HStringReference(
                                                            RuntimeClass_Windows_UI_ViewManagement_UISettings)
                                                            .Get(),
            &settings))) {
        settings->GetColorValue(type, &color);
        *ok = true;
    }
    return color;
}
}

bool hasDarkSystray()
{
    bool ok;
    const auto color = GetThemeColor(ABI::Windows::UI::ViewManagement::UIColorType::UIColorType_Background, &ok);
    if (ok) {
        const auto lightness = 0.5 * (std::max(color.R, std::max(color.G, color.B)) + std::min(color.R, std::min(color.G, color.B))) / 255.0;
        return lightness < 0.5;
    }
    return true;
}

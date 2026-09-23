/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingspanelstyle.h"

#include "settingsswitch.h"

#include <QApplication>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr auto rowLeftMargin = 12;
constexpr auto rowTopMargin = 12;
constexpr auto rowRightMargin = 12;
constexpr auto rowBottomMargin = 12;
constexpr auto rowSpacing = 8;
constexpr auto compactControlHeight = 20;
constexpr auto managedLabelFontScale = 0.9;

bool isSeparator(const QWidget *widget)
{
    return widget && widget->objectName().endsWith(QLatin1String("Separator"));
}

void applyPanelLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

void applyRowLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }

    layout->setContentsMargins(rowLeftMargin, rowTopMargin, rowRightMargin, rowBottomMargin);
    layout->setSpacing(rowSpacing);
}

void applyLayout(QLayout *layout)
{
    if (dynamic_cast<QHBoxLayout *>(layout)) {
        applyRowLayout(layout);
    } else if (dynamic_cast<QVBoxLayout *>(layout)) {
        applyPanelLayout(layout);
    }
}

void applySeparator(QFrame *separator)
{
    separator->setFrameShape(QFrame::NoFrame);
    separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    separator->setFixedHeight(1);
}

void applyCompactSpinBox(QSpinBox *spinBox)
{
    spinBox->setMinimumHeight(compactControlHeight);
    spinBox->setMaximumHeight(compactControlHeight);
    spinBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
}
} // namespace

namespace OCC::SettingsPanelStyle {

void apply(QWidget *root)
{
    if (!root) {
        return;
    }

    const auto panels = root->findChildren<QGroupBox *>();
    for (auto *panel : panels) {
        applyLayout(panel->layout());

        const auto layouts = panel->findChildren<QLayout *>();
        for (auto *layout : layouts) {
            if (layout != panel->layout()) {
                applyLayout(layout);
            }
        }

        const auto frames = panel->findChildren<QFrame *>();
        for (auto *frame : frames) {
            if (isSeparator(frame)) {
                applySeparator(frame);
            }
        }

        const auto spinBoxes = panel->findChildren<QSpinBox *>();
        for (auto *spinBox : spinBoxes) {
            applyCompactSpinBox(spinBox);
        }

        const auto switches = panel->findChildren<SettingsSwitch *>();
        for (auto *settingsSwitch : switches) {
            settingsSwitch->setFixedSize(settingsSwitch->sizeHint());
        }
    }
}

void applyManagedLabelStyle(QLabel *label)
{
    // Scale the inherited font, not the current one, so repeated calls do not keep shrinking the label.
    auto font = label->parentWidget() ? label->parentWidget()->font() : QApplication::font();
    font.setItalic(true);
    font.setPointSizeF(font.pointSizeF() * managedLabelFontScale);
    label->setFont(font);

    label->setContentsMargins(0, 0, rowRightMargin, 0);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

} // namespace OCC::SettingsPanelStyle

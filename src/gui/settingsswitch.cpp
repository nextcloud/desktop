/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsswitch.h"

#include <QPainter>
#include <QPalette>
#include <QPaintEvent>
#include <QPen>
#include <QSizePolicy>

namespace OCC {

SettingsSwitch::SettingsSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize SettingsSwitch::sizeHint() const
{
    return {36, 20};
}

QSize SettingsSwitch::minimumSizeHint() const
{
    return sizeHint();
}

void SettingsSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    constexpr qreal trackWidth = 36.0;
    constexpr qreal trackHeight = 20.0;
    constexpr qreal margin = 2.0;
    constexpr qreal knobSize = trackHeight - 2.0 * margin;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto x = (width() - trackWidth) / 2.0;
    const auto y = (height() - trackHeight) / 2.0;
    const QRectF trackRect{x, y, trackWidth, trackHeight};

    auto trackColor = isChecked() ? palette().highlight().color() : palette().color(QPalette::Button);
    auto borderColor = isChecked() ? palette().highlight().color() : palette().color(QPalette::Mid);
    auto knobColor = palette().color(QPalette::Light);
    if (!isEnabled()) {
        trackColor.setAlphaF(0.45);
        borderColor.setAlphaF(0.45);
        knobColor.setAlphaF(0.65);
    }

    painter.setPen(borderColor);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    const auto knobX = isChecked()
        ? trackRect.right() - margin - knobSize
        : trackRect.left() + margin;
    const QRectF knobRect{knobX, trackRect.top() + margin, knobSize, knobSize};

    painter.setBrush(knobColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(knobRect);

    if (hasFocus()) {
        auto focusColor = palette().highlight().color();
        focusColor.setAlphaF(0.35);
        QPen focusPen(focusColor, 2.0);
        painter.setPen(focusPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(trackRect.adjusted(-2.0, -2.0, 2.0, 2.0), trackHeight / 2.0 + 2.0, trackHeight / 2.0 + 2.0);
    }
}

} // namespace OCC

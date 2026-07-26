/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsswitch.h"

#include <QColor>
#include <QPainter>
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
    return {30, 16};
}

QSize SettingsSwitch::minimumSizeHint() const
{
    return sizeHint();
}

void SettingsSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    constexpr qreal trackWidth = 30.0;
    constexpr qreal trackHeight = 16.0;
    constexpr qreal knobMargin = 3.0;
    constexpr qreal knobSize = 10.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto x = (width() - trackWidth) / 2.0;
    const auto y = (height() - trackHeight) / 2.0;
    const QRectF trackRect{x, y, trackWidth, trackHeight};

    auto trackColor = isChecked() ? QColor(0, 103, 158) : QColor(107, 107, 107);
    auto knobColor = QColor(255, 255, 255);
    auto knobShadowColor = QColor(0, 0, 0, 38);
    if (!isEnabled()) {
        trackColor.setAlphaF(isChecked() ? 0.45 : 0.35);
        knobColor.setAlphaF(0.8);
        knobShadowColor.setAlphaF(0.08);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    const auto knobCenterY = trackRect.center().y();
    const auto knobCenterX = isChecked()
        ? trackRect.right() - knobMargin - knobSize / 2.0
        : trackRect.left() + knobMargin + knobSize / 2.0;
    const QRectF knobRect{knobCenterX - knobSize / 2.0, knobCenterY - knobSize / 2.0, knobSize, knobSize};

    painter.setBrush(knobShadowColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(knobRect.translated(0.0, 0.6));

    painter.setBrush(knobColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(knobRect);

    if (hasFocus()) {
        auto focusColor = palette().highlight().color();
        focusColor.setAlphaF(0.35);
        QPen focusPen(focusColor, 2.0);
        painter.setPen(focusPen);
        painter.setBrush(Qt::NoBrush);
        const auto focusRect = trackRect.adjusted(1.0, 1.0, -1.0, -1.0);
        painter.drawRoundedRect(focusRect, focusRect.height() / 2.0, focusRect.height() / 2.0);
    }
}

} // namespace OCC

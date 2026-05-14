/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsswitch.h"

#include <QPainter>
#include <QPalette>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QString>

namespace OCC {

SettingsSwitch::SettingsSwitch(QWidget *parent)
    : QCheckBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setText(QString());
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

    auto trackColor = isChecked() ? palette().highlight().color() : palette().color(QPalette::Mid);
    auto knobColor = palette().color(QPalette::Base);
    if (!isEnabled()) {
        trackColor.setAlphaF(0.45);
        knobColor.setAlphaF(0.65);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    const auto knobX = isChecked()
        ? trackRect.right() - margin - knobSize
        : trackRect.left() + margin;
    const QRectF knobRect{knobX, trackRect.top() + margin, knobSize, knobSize};

    painter.setBrush(knobColor);
    painter.drawEllipse(knobRect);
}

} // namespace OCC

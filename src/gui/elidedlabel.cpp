/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "elidedlabel.h"

#include <QResizeEvent>

namespace OCC {

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent)
    , _elideMode(Qt::ElideNone)
{
}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
    , _text(text)
    , _elideMode(Qt::ElideNone)
{
}

void ElidedLabel::setText(const QString &text)
{
    _text = text;
    QLabel::setText(text);
    update();
}

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    _elideMode = elideMode;
    update();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);

    QFontMetrics fm = fontMetrics();
    QString elided = fm.elidedText(_text, _elideMode, event->size().width());
    QLabel::setText(elided);
}
}

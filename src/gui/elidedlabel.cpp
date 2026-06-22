/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "elidedlabel.h"

#include <QResizeEvent>

namespace OCC {

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent)
{
}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
    , _text(text)
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

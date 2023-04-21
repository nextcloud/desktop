/*
 * Copyright (C) 2016 by Daniel Molkentin <danimo@owncloud.com>
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

#include <QStyle>
#include <QStyleOptionFrame>

#include "postfixlineedit.h"

namespace OCC {

const int horizontalMargin(4);
const int verticalMargin(4);

PostfixLineEdit::PostfixLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

void PostfixLineEdit::setPostfix(const QString &postfix)
{
    _postfix = postfix;
    QFontMetricsF fm(font());
    QMargins tm = textMargins();
    tm.setRight(tm.right() + fm.horizontalAdvance(_postfix) + verticalMargin);
    setTextMargins(tm);
}

QString PostfixLineEdit::postfix() const
{
    return _postfix;
}

QString PostfixLineEdit::fullText() const
{
    return text() + _postfix;
}

void PostfixLineEdit::setFullText(const QString &text)
{
    QString prefixString = text;
    if (prefixString.endsWith(postfix())) {
        prefixString.chop(postfix().length());
    }
    setText(prefixString);
}

void PostfixLineEdit::paintEvent(QPaintEvent *pe)
{
    QLineEdit::paintEvent(pe);
    QPainter p(this);

    //
    p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
    QFontMetricsF fm(font());
    int start = rect().right() - fm.horizontalAdvance(_postfix);
    QStyleOptionFrame panel;
    initStyleOption(&panel);
    QRect r = style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);
    r.setTop(r.top() + horizontalMargin - 1);
    QRect postfixRect(r);

    postfixRect.setLeft(start - verticalMargin);
    p.drawText(postfixRect, _postfix);
}

} // namespace OCC

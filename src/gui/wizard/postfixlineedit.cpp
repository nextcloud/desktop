/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    tm.setRight(tm.right() + qRound(fm.horizontalAdvance(_postfix)) + verticalMargin);
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
    int start = rect().right() - qRound(fm.horizontalAdvance(_postfix));
    QStyleOptionFrame panel;
    initStyleOption(&panel);
    QRect r = style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);
    r.setTop(r.top() + horizontalMargin - 1);
    QRect postfixRect(r);

    postfixRect.setLeft(start - verticalMargin);
    p.drawText(postfixRect, _postfix);
}

} // namespace OCC

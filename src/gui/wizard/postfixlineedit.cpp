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

#include <QRegExpValidator>
#include <QRegExp>
#include <QDebug>

#include "postfixlineedit.h"

namespace OCC {

// Helper class

/**
 * @brief A QRegExValidator with no Intermediate validation state.
 *
 * Along with a pre-set text in a lineedit, this enforces a certain text
 * to always be present.
 */
class StrictRegExpValidator : public QRegExpValidator
{
public:
    explicit StrictRegExpValidator(const QRegExp& rx, QObject *parent = Q_NULLPTR) :
        QRegExpValidator(rx, parent) {}

    virtual QValidator::State validate(QString& input, int& pos) const Q_DECL_OVERRIDE;
};


QValidator::State StrictRegExpValidator::validate(QString &input, int &pos) const
{
        QValidator::State state = QRegExpValidator::validate(input, pos);
        if (state == QValidator::Intermediate)
            state = QValidator::Invalid;
        return state;
}

// Begin of URLLineEdit impl

PostfixLineEdit::PostfixLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    connect(this, SIGNAL(textChanged(const QString&)), SLOT(slotTextChanged(const QString&)));
}

void PostfixLineEdit::setPostfix(const QString &postfix)
{
    _postfix = postfix;
}

QString PostfixLineEdit::postfix() const
{
    return _postfix;
}

void PostfixLineEdit::focusInEvent(QFocusEvent *ev)
{
    QLineEdit::focusInEvent(ev);
    ensureValidationEngaged();
    setSelection(0 , maxUserInputLength());
}

void PostfixLineEdit::focusOutEvent(QFocusEvent *ev)
{
    QLineEdit::focusOutEvent(ev);
    showPlaceholder();
}

void PostfixLineEdit::slotTextChanged(const QString &)
{
    ensureValidationEngaged();
}

void PostfixLineEdit::mouseReleaseEvent(QMouseEvent *ev)
{
    QLineEdit::mouseReleaseEvent(ev);
    // ensure selections still work
    if (selectedText().isEmpty()) {
        limitCursorPlacement();
    }
}

void PostfixLineEdit::ensureValidationEngaged()
{
    if (_postfix.isEmpty())
        return;

    if (text().isEmpty()) {
        // also called from setText via slotTextChanged
        bool old = blockSignals(true);
        setText(_postfix);
        blockSignals(old);
    }
    if (!validator()) {
        QRegExp rx(QString("*%1").arg(_postfix));
        rx.setPatternSyntax(QRegExp::Wildcard);
        QRegExpValidator *val = new StrictRegExpValidator(rx);
        setValidator(val);
    }
}

void PostfixLineEdit::showPlaceholder()
{
    if (text() == _postfix && !placeholderText().isNull()) {
        setValidator(0);
        setText(QString());
    }
}

int PostfixLineEdit::maxUserInputLength() const
{
    return text().length() - _postfix.length();
}

void PostfixLineEdit::limitCursorPlacement()
{
    if (cursorPosition() > maxUserInputLength()) {
        setCursorPosition(maxUserInputLength());
    }
}

} // namespace OCC

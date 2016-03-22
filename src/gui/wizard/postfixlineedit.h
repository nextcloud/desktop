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

#ifndef OCC_POSTFIXLINEEDIT_H
#define OCC_POSTFIXLINEEDIT_H

#include <QLineEdit>

namespace OCC {

/**
 * @brief A class with a non-removable postfix string.
 *
 * Useful e.g. for setting a fixed domain name.
 */
class PostfixLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    PostfixLineEdit(QWidget *parent = 0);
    /// Sets a non-removeable postfix string
    void setPostfix(const QString &postfix);
    /// @return the currently set postfix. Use @ref text() to retrieve the full text.
    QString postfix() const;

protected:
    void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent *) Q_DECL_OVERRIDE;

private slots:
    void slotTextChanged(const QString&);

private:
    void ensureValidationEngaged();
    void showPlaceholder();
    int maxUserInputLength() const;
    void limitCursorPlacement();
    QString _postfix;
};

} // namespace OCC

#endif // OCC_POSTFIXLINEEDIT_H

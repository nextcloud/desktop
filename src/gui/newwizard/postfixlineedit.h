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
#include <QPaintEvent>
#include <QPainter>

namespace OCC {

/**
 * @brief A lineedit class with a pre-set postfix.
 *
 * Useful e.g. for setting a fixed domain name.
 */

class PostfixLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    PostfixLineEdit(QWidget *parent);

    /** @brief sets an optional postfix shown greyed out */
    void setPostfix(const QString &postfix);
    /** @brief retrives the postfix */
    QString postfix() const;
    /** @brief retrieves combined text() and postfix() */
    QString fullText() const;

    /** @brief sets text() from full text, discarding prefix() */
    void setFullText(const QString &text);

protected:
    void paintEvent(QPaintEvent *pe) override;

private:
    QString _postfix;
};


} // namespace OCC

#endif // OCC_POSTFIXLINEEDIT_H

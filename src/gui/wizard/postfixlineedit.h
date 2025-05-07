/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    /** @brief retrieves the postfix */
    [[nodiscard]] QString postfix() const;
    /** @brief retrieves combined text() and postfix() */
    [[nodiscard]] QString fullText() const;

    /** @brief sets text() from full text, discarding prefix() */
    void setFullText(const QString &text);

protected:
    void paintEvent(QPaintEvent *pe) override;

private:
    QString _postfix;
};


} // namespace OCC

#endif // OCC_POSTFIXLINEEDIT_H

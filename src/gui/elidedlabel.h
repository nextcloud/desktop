/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>

namespace OCC {

/// Label that can elide its text
class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr);
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    [[nodiscard]] const QString &text() const { return _text; }

    void setElideMode(Qt::TextElideMode elideMode);
    [[nodiscard]] Qt::TextElideMode elideMode() const { return _elideMode; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QString _text;
    Qt::TextElideMode _elideMode = Qt::ElideNone;
};
}

#endif

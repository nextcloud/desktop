/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLabel>
#include <QUrl>

namespace OCC {

class LinkLabel : public QLabel
{
    Q_OBJECT
public:
    explicit LinkLabel(QWidget *parent = nullptr);

    void setUrl(const QUrl &url);

signals:
    void clicked();

protected:
    void enterEvent(QEnterEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setFontUnderline(bool value);

    QUrl url;
};

}

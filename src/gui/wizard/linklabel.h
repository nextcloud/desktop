/*
 * Copyright (C) 2021 by Felix Weilbach <felix.weilbach@nextcloud.com>
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
    void enterEvent(QEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setFontUnderline(bool value);

    QUrl url;
};

}

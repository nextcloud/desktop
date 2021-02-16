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

#include "linklabel.h"
#include "guiutility.h"

namespace OCC {

LinkLabel::LinkLabel(QWidget *parent) : QLabel(parent)
{

}

void LinkLabel::setUrl(const QUrl &url)
{
    this->url = url;
}

void LinkLabel::enterEvent(QEvent * /*event*/)
{
    setFontUnderline(true);
    setCursor(Qt::PointingHandCursor);
}

void LinkLabel::leaveEvent(QEvent * /*event*/)
{
    setFontUnderline(false);
    setCursor(Qt::ArrowCursor);
}

void LinkLabel::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    if (url.isValid()) {
        Utility::openBrowser(url);
    }

    emit clicked();
}

void LinkLabel::setFontUnderline(bool value)
{
    auto labelFont = font();
    labelFont.setUnderline(value);
    setFont(labelFont);
}

}

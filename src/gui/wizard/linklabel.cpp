/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

void LinkLabel::enterEvent(QEnterEvent * /*event*/)
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

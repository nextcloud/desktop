/*
 * Copyright (C) 2023 by Fabian Müller <fmueller@owncloud.com>
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

#include <QScopedPointer>
#include <QWidget>

namespace Ui {
class UpdateDownloadedWidget;
}

namespace OCC {

class UpdateDownloadedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateDownloadedWidget(QWidget *parent, const QString &statusMessage);
    ~UpdateDownloadedWidget();

public Q_SLOTS:
    void accept();
    void reject();

Q_SIGNALS:
    void accepted();
    void finished();

private:
    ::Ui::UpdateDownloadedWidget *_ui;
};

}

/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QLocalSocket>

namespace Ui {
class Window;
}

class Window : public QWidget
{
    Q_OBJECT

public:
    explicit Window(QIODevice *dev, QWidget *parent = 0);
    ~Window();

public slots:
    void receive();
    void receiveError(QLocalSocket::LocalSocketError);

private slots:
    void handleReturn();

private:
    void addDefaultItems();
    Ui::Window *ui;
    QIODevice *device;
};

#endif // WINDOW_H

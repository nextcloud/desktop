/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_NETWORKSETTINGS_H
#define MIRALL_NETWORKSETTINGS_H

#include <QWidget>


namespace OCC {

namespace Ui {
    class NetworkSettings;
}

/**
 * @brief The NetworkSettings class
 * @ingroup gui
 */
class NetworkSettings : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkSettings(QWidget *parent = 0);
    ~NetworkSettings();
    QSize sizeHint() const;

private slots:
    void saveProxySettings();
    void saveBWLimitSettings();

private:
    void loadProxySettings();
    void loadBWLimitSettings();

    Ui::NetworkSettings *_ui;
};


} // namespace OCC
#endif // MIRALL_NETWORKSETTINGS_H

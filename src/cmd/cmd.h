/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CMD_H
#define CMD_H

#include <QObject>

/**
 * @brief Helper class for command line client
 * @ingroup cmd
 */
class Cmd : public QObject
{
    Q_OBJECT
public:
    Cmd()
        : QObject()
    {
    }
public slots:
    void transmissionProgressSlot()
    {
    }
};

#endif

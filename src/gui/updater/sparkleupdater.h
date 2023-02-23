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

#ifndef SPARKLEUPDATER_H
#define SPARKLEUPDATER_H

#include "updater/updater.h"

#include <QObject>

namespace OCC {

class SparkleUpdater : public Updater
{
    Q_OBJECT

public:
    enum class State {
        Unknown = 0,
        Idle,
        Working,
        AwaitingUserInput
    };

    SparkleUpdater(const QUrl &appCastUrl);
    ~SparkleUpdater();

    static bool autoUpdaterAllowed();

    void setUpdateUrl(const QUrl &url);

    // unused in this updater
    void checkForUpdate() override;
    void backgroundCheckForUpdate() override;
    bool handleStartup() override { return false; }

    QString statusString() const;
    State state() const;

    class SparkleInterface;

signals:
    void statusChanged();

private:
    std::unique_ptr<SparkleInterface> _interface;
    QString _statusString;
    State _state = State::Unknown;
    friend class SparkleInterface;
};

} // namespace OCC

#endif // SPARKLEUPDATER_H

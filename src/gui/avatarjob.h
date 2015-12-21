/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef AVATARJOB_H
#define AVATARJOB_H

#include "networkjobs.h"
#include "accountfwd.h"

namespace OCC {

/**
 * @brief Job to fetch an avatar for a user
 * @ingroup gui
 */
class AvatarJob2 : public AbstractNetworkJob {
    Q_OBJECT
public:
    /**
     * @param userId The user for which to obtain the avatar
     * @param size The size of the avatar (square so size*size)
     * @param account For which account to obtain the avatar
     * @param parent Parent of the object
     */
    explicit AvatarJob2(const QString& userId, int size, AccountPtr account, QObject* parent = 0);

public slots:
    /* Start the job */
    void start() Q_DECL_OVERRIDE;
signals:
    /**
     * Signal that the job is done and returned an avatar
     *
     * @param reply the content of the reply
     * @param the mimetype (set by the server)
     */
    void avatarReady(QByteArray reply, QString mimeType);

    /**
     * Signal that the job is done but the server did not return
     * an avatar
     *
     * @param statusCode The status code returned by the server
     */
    void avatarNotAvailable(int statusCode);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
};

}

#endif // AVATAR_H

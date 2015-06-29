/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include <networkjobs.h>

namespace OCC
{

/**
 * @brief Fetch the user name of the shibboleth connection
 * @ingroup gui
 */
class ShibbolethUserJob : public JsonApiJob {
    Q_OBJECT
public:
    explicit ShibbolethUserJob(AccountPtr account, QObject* parent = 0);

signals:
    // is always emitted when the job is finished.  user is empty in case of error.
    void userFetched(const QString &user);

private slots:
    void slotJsonRecieved(const QVariantMap &);
};


} // namespace OCC


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


#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include "owncloudlib.h"

#include <QVariantMap>

namespace OCC {

/**
 * @brief The Capabilities class represent the capabilities of an ownCloud
 * server
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Capabilities {

public:
    Capabilities(const QVariantMap &capabilities);

    bool publicLinkEnforcePassword() const;
    bool publicLinkEnforceExpireDate() const;
    int  publicLinkExpireDateDays() const;

private:
    QVariantMap _capabilities;
};

}

#endif //CAPABILITIES_H

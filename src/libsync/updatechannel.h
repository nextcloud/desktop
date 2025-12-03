/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UPDATECHANNEL_H
#define UPDATECHANNEL_H

#include "owncloudlib.h"

#include <QList>
#include <QString>

class OWNCLOUDSYNC_EXPORT UpdateChannel
{
public:
    enum class ChannelName : unsigned int {
        // Values are assigned by stability (higher means more stable).
        invalid = 0,
        daily = 1,
        beta = 2,
        stable = 3,
        enterprise = 4
    };

    [[nodiscard]] ChannelName channelName() const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString toString() const;
    std::strong_ordering operator<=>(const UpdateChannel &rhs) const;

    [[nodiscard]] static UpdateChannel fromString(const QString &channelName);
    [[nodiscard]] static UpdateChannel mostStable(const UpdateChannel &channelA, const UpdateChannel &channelB);

    [[nodiscard]] static const QList<UpdateChannel> &defaultUpdateChannelList();
    [[nodiscard]] static const UpdateChannel &defaultUpdateChannel();
    [[nodiscard]] static const QList<UpdateChannel> &enterpriseUpdateChannelsList();
    [[nodiscard]] static const UpdateChannel &defaultEnterpriseChannel();

    static const UpdateChannel Invalid;
    static const UpdateChannel Daily;
    static const UpdateChannel Beta;
    static const UpdateChannel Stable;
    static const UpdateChannel Enterprise;

private:
    UpdateChannel(const ChannelName &channelName);

    ChannelName _channelName;
};

#endif // UPDATECHANNEL_H

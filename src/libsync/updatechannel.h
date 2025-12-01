/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2025 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UPDATECHANNEL_H
#define UPDATECHANNEL_H

#include <QString>

class UpdateChannel
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

    ChannelName channelName() const;
    bool isValid() const;
    QString toString() const;
    std::strong_ordering operator<=>(const UpdateChannel &rhs) const;

    static UpdateChannel fromString(const QString &channelName);
    static UpdateChannel mostStable(const UpdateChannel &a, const UpdateChannel &b);

    static const UpdateChannel Invalid;
    static const UpdateChannel Daily;
    static const UpdateChannel Beta;
    static const UpdateChannel Stable;
    static const UpdateChannel Enterprise;

private:
    UpdateChannel();
    UpdateChannel &setChannelName(ChannelName channelName);

    ChannelName _channelName;
};

#endif // UPDATECHANNEL_H

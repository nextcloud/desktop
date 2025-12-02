/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updatechannel.h"

#include <QMap>

UpdateChannel::ChannelName UpdateChannel::channelName() const
{
    return _channelName;
}

bool UpdateChannel::isValid() const
{
    return _channelName != ChannelName::invalid;
}

QString UpdateChannel::toString() const
{
    using enum ChannelName;
    static const QMap<ChannelName, QString> enumToNameLut = {{invalid, "invalid"},
                                                             {daily, "daily"},
                                                             {beta, "beta"},
                                                             {stable, "stable"},
                                                             {enterprise, "enterprise"}};
    return enumToNameLut.value(_channelName);
}

std::strong_ordering UpdateChannel::operator<=>(const UpdateChannel &rhs) const = default;

UpdateChannel UpdateChannel::mostStable(const UpdateChannel &channelA, const UpdateChannel &channelB)
{
    return std::max(channelA, channelB);
}

const QList<UpdateChannel> &UpdateChannel::defaultUpdateChannelList()
{
    static const QList<UpdateChannel> list{UpdateChannel::Stable, UpdateChannel::Beta, UpdateChannel::Daily};
    return list;
}

const UpdateChannel &UpdateChannel::defaultUpdateChannel()
{
    static const auto channel = UpdateChannel::Stable;
    return channel;
}

const QList<UpdateChannel> &UpdateChannel::enterpriseUpdateChannelsList()
{
    static const QList<UpdateChannel> list{UpdateChannel::Stable, UpdateChannel::Enterprise};
    return list;
}

const UpdateChannel &UpdateChannel::defaultEnterpriseChannel()
{
    static const auto channel = UpdateChannel::Enterprise;
    return channel;
}

const UpdateChannel UpdateChannel::Invalid = UpdateChannel::fromString("invalid");
const UpdateChannel UpdateChannel::Daily = UpdateChannel::fromString("daily");
const UpdateChannel UpdateChannel::Beta = UpdateChannel::fromString("beta");
const UpdateChannel UpdateChannel::Stable = UpdateChannel::fromString("stable");
const UpdateChannel UpdateChannel::Enterprise = UpdateChannel::fromString("enterprise");

UpdateChannel UpdateChannel::fromString(const QString &channelName) // static
{
    using enum ChannelName;
    static const QMap<QString, ChannelName> nameToEnumLut = {{"invalid", invalid},
                                                             {"daily", daily},
                                                             {"beta", beta},
                                                             {"stable", stable},
                                                             {"enterprise", enterprise}};
    auto channelEnum = nameToEnumLut.contains(channelName) ? nameToEnumLut.value(channelName) : invalid;
    return UpdateChannel(channelEnum);
}

UpdateChannel::UpdateChannel(const ChannelName &channelName)
    : _channelName(channelName)
{
}

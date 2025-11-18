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

UpdateChannel UpdateChannel::fromString(const QString &channelName) // static
{
    using enum ChannelName;
    static const QMap<QString, ChannelName> nameToEnumLut = {{"invalid", invalid},
                                                             {"daily", daily},
                                                             {"beta", beta},
                                                             {"stable", stable},
                                                             {"enterprise", enterprise}};
    auto channelEnum = nameToEnumLut.contains(channelName) ? nameToEnumLut.value(channelName) : invalid;
    return UpdateChannel().setChannelName(channelEnum);
}

UpdateChannel::UpdateChannel()
    : _channelName(ChannelName::invalid)
{
}

UpdateChannel &UpdateChannel::setChannelName(ChannelName channelName)
{
    _channelName = channelName;
    return *this;
}

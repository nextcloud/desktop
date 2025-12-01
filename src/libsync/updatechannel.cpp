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

UpdateChannel UpdateChannel::mostStable(const UpdateChannel &a, const UpdateChannel &b)
{
    return std::max(a, b);
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

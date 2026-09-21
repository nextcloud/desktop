/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixtureuserstatusmodel.h"
#include "fakeuserstatusconnector.h"
namespace OCC::DocAssets
{
namespace
{
std::shared_ptr<UserStatusConnector> connector()
{
    auto result = std::make_shared<FakeUserStatusConnector>();
    result->setFakeUserStatus(
        UserStatus(QStringLiteral("documentation"), QStringLiteral("Reviewing the project plan"), QStringLiteral(""), UserStatus::OnlineStatus::Online, false));
    constexpr auto secondsPerMinute = 60;
    auto meetingExpiry = ClearAt{};
    meetingExpiry._period = 60 * secondsPerMinute;
    auto commutingExpiry = ClearAt{};
    commutingExpiry._period = 30 * secondsPerMinute;
    auto shortBreakExpiry = ClearAt{};
    shortBreakExpiry._period = 15 * secondsPerMinute;
    auto todayExpiry = ClearAt{};
    todayExpiry._type = ClearAtType::EndOf;
    todayExpiry._endof = QStringLiteral("day");
    // Visible defaults from the server's user_status PredefinedStatusService.
    result->setFakePredefinedStatuses({
        UserStatus(QStringLiteral("meeting"), QStringLiteral("In a meeting"), QStringLiteral("📅"), UserStatus::OnlineStatus::Online, true, meetingExpiry),
        UserStatus(QStringLiteral("commuting"), QStringLiteral("Commuting"), QStringLiteral("🚌"), UserStatus::OnlineStatus::Online, true, commutingExpiry),
        UserStatus(QStringLiteral("be-right-back"),
                   QStringLiteral("Be right back"),
                   QStringLiteral("⏳"),
                   UserStatus::OnlineStatus::Online,
                   true,
                   shortBreakExpiry),
        UserStatus(QStringLiteral("remote-work"),
                   QStringLiteral("Working remotely"),
                   QStringLiteral("🏡"),
                   UserStatus::OnlineStatus::Online,
                   true,
                   todayExpiry),
        UserStatus(QStringLiteral("sick-leave"), QStringLiteral("Out sick"), QStringLiteral("🤒"), UserStatus::OnlineStatus::Online, true, todayExpiry),
        UserStatus(QStringLiteral("vacationing"), QStringLiteral("Vacationing"), QStringLiteral("🌴"), UserStatus::OnlineStatus::Online, true),
    });
    return result;
}
}
FixtureUserStatusModel::FixtureUserStatusModel(QObject *parent)
    : UserStatusSelectorModel(connector(), parent)
{
}
}

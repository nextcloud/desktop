/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "userstatusconnector.h"
#include "userstatusselectormodel.h"
#include "logger.h"

#include <QTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QStandardPaths>

#include <memory>

class FakeUserStatusConnector : public OCC::UserStatusConnector
{
public:
    void fetchUserStatus() override
    {
        if (_couldNotFetchUserStatus) {
            emit error(Error::CouldNotFetchUserStatus);
            return;
        } else if (_userStatusNotSupported) {
            emit error(Error::UserStatusNotSupported);
            return;
        } else if (_emojisNotSupported) {
            emit error(Error::EmojisNotSupported);
            return;
        }

        emit userStatusFetched(_userStatus);
    }

    void fetchPredefinedStatuses() override
    {
        if (_couldNotFetchPredefinedUserStatuses) {
            emit error(Error::CouldNotFetchPredefinedUserStatuses);
            return;
        }
        emit predefinedStatusesFetched(_predefinedStatuses);
    }

    void setUserStatus(const OCC::UserStatus &userStatus) override
    {
        if (_couldNotSetUserStatusMessage) {
            emit error(Error::CouldNotSetUserStatus);
            return;
        }

        _userStatusSetByCallerOfSetUserStatus = userStatus;
        emit UserStatusConnector::userStatusSet();
    }

    void clearMessage() override
    {
        if (_couldNotClearUserStatusMessage) {
            emit error(Error::CouldNotClearMessage);
        } else {
            _isMessageCleared = true;
        }
    }

    [[nodiscard]] OCC::UserStatus userStatus() const override
    {
        return {}; // Not implemented
    }

    [[nodiscard]] bool supportsBusyStatus() const override
    {
        return true;
    }

    void setFakeUserStatus(const OCC::UserStatus &userStatus)
    {
        _userStatus = userStatus;
    }

    void setFakePredefinedStatuses(
        const QVector<OCC::UserStatus> &statuses)
    {
        _predefinedStatuses = statuses;
    }

    [[nodiscard]] OCC::UserStatus userStatusSetByCallerOfSetUserStatus() const { return _userStatusSetByCallerOfSetUserStatus; }

    [[nodiscard]] bool messageCleared() const { return _isMessageCleared; }

    void setErrorCouldNotFetchPredefinedUserStatuses(bool value)
    {
        _couldNotFetchPredefinedUserStatuses = value;
    }

    void setErrorCouldNotFetchUserStatus(bool value)
    {
        _couldNotFetchUserStatus = value;
    }

    void setErrorCouldNotSetUserStatusMessage(bool value)
    {
        _couldNotSetUserStatusMessage = value;
    }

    void setErrorUserStatusNotSupported(bool value)
    {
        _userStatusNotSupported = value;
    }

    void setErrorEmojisNotSupported(bool value)
    {
        _emojisNotSupported = value;
    }

    void setErrorCouldNotClearUserStatusMessage(bool value)
    {
        _couldNotClearUserStatusMessage = value;
    }

private:
    OCC::UserStatus _userStatusSetByCallerOfSetUserStatus;
    OCC::UserStatus _userStatus;
    QVector<OCC::UserStatus> _predefinedStatuses;
    bool _isMessageCleared = false;
    bool _couldNotFetchPredefinedUserStatuses = false;
    bool _couldNotFetchUserStatus = false;
    bool _couldNotSetUserStatusMessage = false;
    bool _userStatusNotSupported = false;
    bool _emojisNotSupported = false;
    bool _couldNotClearUserStatusMessage = false;
};

class FakeDateTimeProvider : public OCC::DateTimeProvider
{
public:
    void setCurrentDateTime(const QDateTime &dateTime) { _dateTime = dateTime; }

    [[nodiscard]] QDateTime currentDateTime() const override { return _dateTime; }

    [[nodiscard]] QDate currentDate() const override { return _dateTime.date(); }

private:
    QDateTime _dateTime;
};

static QVector<OCC::UserStatus>
createFakePredefinedStatuses(const QDateTime &currentTime)
{
    QVector<OCC::UserStatus> statuses;

    const QString userStatusId("fake-id");
    const QString userStatusMessage("Predefined status");
    const QString userStatusIcon("🏖");
    const OCC::UserStatus::OnlineStatus userStatusState(OCC::UserStatus::OnlineStatus::Online);
    const bool userStatusMessagePredefined(true);
    OCC::Optional<OCC::ClearAt> userStatusClearAt;
    OCC::ClearAt clearAt;
    clearAt._type = OCC::ClearAtType::Timestamp;
    clearAt._timestamp = currentTime.addSecs(60 * 60).toSecsSinceEpoch();
    userStatusClearAt = clearAt;

    statuses.append({userStatusId, userStatusMessage, userStatusIcon,
        userStatusState, userStatusMessagePredefined, userStatusClearAt});

    return statuses;
}

static QDateTime createDateTime(int year = 2021, int month = 7, int day = 27,
    int hour = 12, int minute = 0, int second = 0)
{
    QDate fakeDate(year, month, day);
    QTime fakeTime(hour, minute, second);
    QDateTime fakeDateTime;

    fakeDateTime.setDate(fakeDate);
    fakeDateTime.setTime(fakeTime);

    return fakeDateTime;
}

class TestSetUserStatusDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testCtor_fetchStatusAndPredefinedStatuses()
    {
        const QDateTime currentDateTime(QDateTime::currentDateTimeUtc());

        const QString userStatusId("fake-id");
        const QString userStatusMessage("Some status");
        const QString userStatusIcon("❤");
        const OCC::UserStatus::OnlineStatus userStatusState(OCC::UserStatus::OnlineStatus::DoNotDisturb);
        const bool userStatusMessagePredefined(false);
        OCC::Optional<OCC::ClearAt> userStatusClearAt;
        {
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentDateTime.addDays(1).toSecsSinceEpoch();
            userStatusClearAt = clearAt;
        }

        const OCC::UserStatus userStatus(userStatusId, userStatusMessage,
            userStatusIcon, userStatusState, userStatusMessagePredefined, userStatusClearAt);

        const auto fakePredefinedStatuses = createFakePredefinedStatuses(createDateTime());

        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
        fakeDateTimeProvider->setCurrentDateTime(currentDateTime);
        fakeUserStatusJob->setFakeUserStatus(userStatus);
        fakeUserStatusJob->setFakePredefinedStatuses(fakePredefinedStatuses);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob, std::move(fakeDateTimeProvider));

        // Was user status set correctly?
        QCOMPARE(model.userStatusMessage(), userStatusMessage);
        QCOMPARE(model.userStatusEmoji(), userStatusIcon);
        QCOMPARE(model.onlineStatus(), userStatusState);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 day(s)"));

        // Were predefined statuses fetched correctly?
        const auto predefinedStatusesCount = model.predefinedStatuses().count();
        QCOMPARE(predefinedStatusesCount, fakePredefinedStatuses.size());
        for (int i = 0; i < predefinedStatusesCount; ++i) {
            const auto predefinedStatus = model.predefinedStatuses()[i];
            QCOMPARE(predefinedStatus.id(),
                fakePredefinedStatuses[i].id());
            QCOMPARE(predefinedStatus.message(),
                fakePredefinedStatuses[i].message());
            QCOMPARE(predefinedStatus.icon(),
                fakePredefinedStatuses[i].icon());
            QCOMPARE(predefinedStatus.messagePredefined(),
                fakePredefinedStatuses[i].messagePredefined());
        }
    }

    void testCtor_noStatusSet_showSensibleDefaults()
    {
        OCC::UserStatusSelectorModel model(nullptr, nullptr);

        QCOMPARE(model.userStatusMessage(), "");
        QCOMPARE(model.userStatusEmoji(), "");
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Don't clear"));
    }

    void testCtor_fetchStatusButNoStatusSet_showSensibleDefaults()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setFakeUserStatus({ "", "", "",
            OCC::UserStatus::OnlineStatus::Offline, false, {} });
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.onlineStatus(), OCC::UserStatus::OnlineStatus::Offline);
        QCOMPARE(model.userStatusMessage(), "");
        QCOMPARE(model.userStatusEmoji(), "");
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Don't clear"));
    }

    void testSetOnlineStatus_emiUserStatusChanged()
    {
        const OCC::UserStatus::OnlineStatus onlineStatus(OCC::UserStatus::OnlineStatus::Invisible);
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy userStatusChangedSpy(&model,
            &OCC::UserStatusSelectorModel::userStatusChanged);

        model.setOnlineStatus(onlineStatus);

        QCOMPARE(userStatusChangedSpy.count(), 1);
    }

    void testSetUserStatus_setCustomMessage_userStatusSetCorrect()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy finishedSpy(&model, &OCC::UserStatusSelectorModel::finished);

        const QString userStatusMessage("Some status");
        const QString userStatusIcon("❤");
        const OCC::UserStatus::OnlineStatus userStatusState(OCC::UserStatus::OnlineStatus::Online);

        model.setOnlineStatus(userStatusState);
        model.setUserStatusMessage(userStatusMessage);
        model.setUserStatusEmoji(userStatusIcon);
        model.setClearAt(OCC::UserStatusSelectorModel::ClearStageType::HalfHour);

        model.setUserStatus();
        QCOMPARE(finishedSpy.count(), 1);

        const auto userStatusSet = fakeUserStatusJob->userStatusSetByCallerOfSetUserStatus();
        QCOMPARE(userStatusSet.icon(), userStatusIcon);
        QCOMPARE(userStatusSet.message(), userStatusMessage);
        QCOMPARE(userStatusSet.state(), userStatusState);
        QCOMPARE(userStatusSet.messagePredefined(), false);
        const auto clearAt = userStatusSet.clearAt();
        QVERIFY(clearAt.isValid());
        QCOMPARE(clearAt->_type, OCC::ClearAtType::Period);
        QCOMPARE(clearAt->_period, 60 * 30);
    }

    void testSetUserStatusMessage_predefinedStatusWasSet_userStatusSetCorrect()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setFakePredefinedStatuses(createFakePredefinedStatuses(createDateTime()));
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        model.setPredefinedStatus(model.predefinedStatuses()[0]);
        QSignalSpy finishedSpy(&model, &OCC::UserStatusSelectorModel::finished);

        const QString userStatusMessage("Some status");
        const OCC::UserStatus::OnlineStatus userStatusState(OCC::UserStatus::OnlineStatus::Online);

        model.setOnlineStatus(userStatusState);
        model.setUserStatusMessage(userStatusMessage);
        model.setClearAt(OCC::UserStatusSelectorModel::ClearStageType::HalfHour);

        model.setUserStatus();
        QCOMPARE(finishedSpy.count(), 1);

        const auto userStatusSet = fakeUserStatusJob->userStatusSetByCallerOfSetUserStatus();
        QCOMPARE(userStatusSet.message(), userStatusMessage);
        QCOMPARE(userStatusSet.state(), userStatusState);
        QCOMPARE(userStatusSet.messagePredefined(), false);
        const auto clearAt = userStatusSet.clearAt();
        QVERIFY(clearAt.isValid());
        QCOMPARE(clearAt->_type, OCC::ClearAtType::Period);
        QCOMPARE(clearAt->_period, 60 * 30);
    }

    void testSetUserStatusEmoji_predefinedStatusWasSet_userStatusSetCorrect()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setFakePredefinedStatuses(createFakePredefinedStatuses(createDateTime()));
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        model.setPredefinedStatus(model.predefinedStatuses()[0]);
        QSignalSpy finishedSpy(&model, &OCC::UserStatusSelectorModel::finished);

        const QString userStatusIcon("❤");
        const OCC::UserStatus::OnlineStatus userStatusState(OCC::UserStatus::OnlineStatus::Online);

        model.setOnlineStatus(userStatusState);
        model.setUserStatusEmoji(userStatusIcon);
        model.setClearAt(OCC::UserStatusSelectorModel::ClearStageType::HalfHour);

        model.setUserStatus();
        QCOMPARE(finishedSpy.count(), 1);

        const auto userStatusSet = fakeUserStatusJob->userStatusSetByCallerOfSetUserStatus();
        QCOMPARE(userStatusSet.icon(), userStatusIcon);
        QCOMPARE(userStatusSet.state(), userStatusState);
        QCOMPARE(userStatusSet.messagePredefined(), false);
        const auto clearAt = userStatusSet.clearAt();
        QVERIFY(clearAt.isValid());
        QCOMPARE(clearAt->_type, OCC::ClearAtType::Period);
        QCOMPARE(clearAt->_period, 60 * 30);
    }

    void testSetPredefinedStatus_emitUserStatusChangedAndSetUserStatus()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
        const auto currentTime = createDateTime();
        fakeDateTimeProvider->setCurrentDateTime(currentTime);
        const auto fakePredefinedStatuses = createFakePredefinedStatuses(currentTime);
        fakeUserStatusJob->setFakePredefinedStatuses(fakePredefinedStatuses);
        OCC::UserStatusSelectorModel model(std::move(fakeUserStatusJob),
            std::move(fakeDateTimeProvider));

        QSignalSpy userStatusChangedSpy(&model,
            &OCC::UserStatusSelectorModel::userStatusChanged);
        QSignalSpy clearAtDisplayStringChangedSpy(&model,
            &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto fakePredefinedUserStatusIndex = 0;
        model.setPredefinedStatus(model.predefinedStatuses()[fakePredefinedUserStatusIndex]);

        QCOMPARE(userStatusChangedSpy.count(), 1);
        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);

        // Was user status set correctly?
        const auto fakePredefinedUserStatus = fakePredefinedStatuses[fakePredefinedUserStatusIndex];
        QCOMPARE(model.userStatusMessage(), fakePredefinedUserStatus.message());
        QCOMPARE(model.userStatusEmoji(), fakePredefinedUserStatus.icon());
        QCOMPARE(model.onlineStatus(), fakePredefinedUserStatus.state());
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 hour(s)"));
    }

    void testSetClear_setClearAtStage0_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::DontClear;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Don't clear"));
    }

    void testSetClear_setClearAtStage1_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::HalfHour;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("30 minute(s)"));
    }

    void testSetClear_setClearAtStage2_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::OneHour;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 hour(s)"));
    }

    void testSetClear_setClearAtStage3_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::FourHour;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("4 hour(s)"));
    }

    void testSetClear_setClearAtStage4_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::Today;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Today"));
    }

    void testSetClear_setClearAtStage5_emitclearAtDisplayStringChangedAndClearAtSet()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        QSignalSpy clearAtDisplayStringChangedSpy(&model, &OCC::UserStatusSelectorModel::clearAtDisplayStringChanged);

        const auto clearAtToSet = OCC::UserStatusSelectorModel::ClearStageType::Week;
        model.setClearAt(clearAtToSet);

        QCOMPARE(clearAtDisplayStringChangedSpy.count(), 1);
        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("This week"));
    }

    void testClearAtStages()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Don't clear"));
        const auto clearStageTypes = model.clearStageTypes();
        QCOMPARE(clearStageTypes.count(), 6);

        QCOMPARE(clearStageTypes[0].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("Don't clear"));
        QCOMPARE(clearStageTypes[1].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("30 minutes"));
        QCOMPARE(clearStageTypes[2].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("1 hour"));
        QCOMPARE(clearStageTypes[3].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("4 hours"));
        QCOMPARE(clearStageTypes[4].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("Today"));
        QCOMPARE(clearStageTypes[5].value<QVariantMap>()[QStringLiteral("display")], QStringLiteral("This week"));
    }

    void testClearAt_clearAtTimestamp()
    {
        const auto currentTime = createDateTime();
        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addSecs(30).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Less than a minute"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addSecs(60).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 minute(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addSecs(60 * 30).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("30 minute(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addSecs(60 * 60).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 hour(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addSecs(60 * 60 * 4).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("4 hour(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addDays(1).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 day(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Timestamp;
            clearAt._timestamp = currentTime.addDays(7).toSecsSinceEpoch();
            userStatus.setClearAt(clearAt);

            auto fakeDateTimeProvider = std::make_unique<FakeDateTimeProvider>();
            fakeDateTimeProvider->setCurrentDateTime(currentTime);

            OCC::UserStatusSelectorModel model(userStatus, std::move(fakeDateTimeProvider));

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("7 day(s)"));
        }
    }

    void testClearAt_clearAtEndOf()
    {
        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::EndOf;
            clearAt._endof = "day";
            userStatus.setClearAt(clearAt);

            OCC::UserStatusSelectorModel model(userStatus);

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("Today"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::EndOf;
            clearAt._endof = "week";
            userStatus.setClearAt(clearAt);

            OCC::UserStatusSelectorModel model(userStatus);

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("This week"));
        }
    }

    void testClearAt_clearAtAfterPeriod()
    {
        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Period;
            clearAt._period = 60 * 30;
            userStatus.setClearAt(clearAt);

            OCC::UserStatusSelectorModel model(userStatus);

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("30 minute(s)"));
        }

        {
            OCC::UserStatus userStatus;
            OCC::ClearAt clearAt;
            clearAt._type = OCC::ClearAtType::Period;
            clearAt._period = 60 * 60;
            userStatus.setClearAt(clearAt);

            OCC::UserStatusSelectorModel model(userStatus);

            QCOMPARE(model.clearAtDisplayString(), QStringLiteral("1 hour(s)"));
        }
    }

    void testClearUserStatus()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        model.clearUserStatus();

        QVERIFY(fakeUserStatusJob->messageCleared());
    }

    void testError_couldNotFetchPredefinedStatuses_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorCouldNotFetchPredefinedUserStatuses(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Could not fetch predefined statuses. Make sure you are connected to the server."));
    }

    void testError_couldNotFetchUserStatus_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorCouldNotFetchUserStatus(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Could not fetch status. Make sure you are connected to the server."));
    }

    void testError_userStatusNotSupported_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorUserStatusNotSupported(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Status feature is not supported. You will not be able to set your status."));
    }

    void testError_couldSetUserStatus_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorCouldNotSetUserStatusMessage(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        model.setUserStatus();

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Could not set status. Make sure you are connected to the server."));
    }

    void testError_emojisNotSupported_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorEmojisNotSupported(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Emojis are not supported. Some status functionality may not work."));
    }

    void testError_couldNotClearMessage_emitError()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        fakeUserStatusJob->setErrorCouldNotClearUserStatusMessage(true);
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);
        model.clearUserStatus();

        QCOMPARE(model.errorMessage(),
            QStringLiteral("Could not clear status message. Make sure you are connected to the server."));
    }

    void testError_setUserStatus_clearErrorMessage()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        fakeUserStatusJob->setErrorCouldNotSetUserStatusMessage(true);
        model.setUserStatus();
        QVERIFY(!model.errorMessage().isEmpty());
        fakeUserStatusJob->setErrorCouldNotSetUserStatusMessage(false);
        model.setUserStatus();
        QVERIFY(model.errorMessage().isEmpty());
    }

    void testError_clearUserStatus_clearErrorMessage()
    {
        auto fakeUserStatusJob = std::make_shared<FakeUserStatusConnector>();
        OCC::UserStatusSelectorModel model(fakeUserStatusJob);

        fakeUserStatusJob->setErrorCouldNotSetUserStatusMessage(true);
        model.setUserStatus();
        QVERIFY(!model.errorMessage().isEmpty());
        fakeUserStatusJob->setErrorCouldNotSetUserStatusMessage(false);
        model.clearUserStatus();
        QVERIFY(model.errorMessage().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSetUserStatusDialog)
#include "testsetuserstatusdialog.moc"

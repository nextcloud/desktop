/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistant/assistantconversationmodel.h"
#include "assistant/assistantmessagemodel.h"
#include "assistant/assistanttaskmodel.h"
#include "assistant/assistanttasktypemodel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace OCC;

namespace
{

QJsonDocument ocsResponse(const QJsonValue &data)
{
    return QJsonDocument{QJsonObject{
        {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), data}}},
    }};
}

QJsonObject textShape(const QString &key)
{
    return {{key, QJsonObject{{QStringLiteral("type"), QStringLiteral("Text")}}}};
}

QJsonObject task(qint64 id, const QString &type, const QString &status, const QString &appId = QStringLiteral("assistant"))
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("appId"), appId},
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("input"), QJsonObject{{QStringLiteral("text"), QStringLiteral("Question")}}}}},
        {QStringLiteral("output"), QJsonObject{{QStringLiteral("output"), QStringLiteral("Answer")}}},
        {QStringLiteral("status"), status},
        {QStringLiteral("lastUpdated"), 1},
    };
}

QJsonObject conversation(qint64 id, const QString &title = {})
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("timestamp"), 1},
    };
}

QJsonObject message(qint64 id, const QString &sessionKey, qint64 sessionId, const QString &role, const QString &content)
{
    return {
        {QStringLiteral("id"), id},
        {sessionKey, sessionId},
        {QStringLiteral("role"), role},
        {QStringLiteral("content"), content},
        {QStringLiteral("timestamp"), 1},
    };
}

}

class TestAssistantModels : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void taskTypesFilterSortAndExposeMetadata()
    {
        AssistantTaskTypeModel model;
        const auto types = QJsonObject{
            {QStringLiteral("core:text2text:summarize"),
             QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("Summarize")},
                 {QStringLiteral("description"), QStringLiteral("Shorten text")},
                 {QStringLiteral("inputShape"), textShape(QStringLiteral("input"))},
                 {QStringLiteral("outputShape"), textShape(QStringLiteral("output"))},
             }},
            {QStringLiteral("core:text2text:chat"), QJsonObject{}},
            {QStringLiteral("core:text2text:translate"), QJsonObject{{QStringLiteral("name"), QStringLiteral("Translate")}}},
            {QStringLiteral("core:audio2text:transcribe"),
             QJsonObject{
                 {QStringLiteral("inputShape"), QJsonObject{{QStringLiteral("audio"), QJsonObject{{QStringLiteral("type"), QStringLiteral("Audio")}}}}},
                 {QStringLiteral("outputShape"), textShape(QStringLiteral("output"))},
             }},
        };

        model.replaceFromResponse(ocsResponse(QJsonObject{{QStringLiteral("types"), types}}));

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.firstTypeId(), QStringLiteral("core:text2text:chat"));
        QVERIFY(model.contains(QStringLiteral("core:text2text:summarize")));
        QVERIFY(!model.contains(QStringLiteral("core:audio2text:transcribe")));
        QCOMPARE(model.nameForType(QStringLiteral("core:text2text:chat")), QStringLiteral("Chat"));
        QCOMPARE(model.descriptionForType(QStringLiteral("core:text2text:summarize")), QStringLiteral("Shorten text"));
        QCOMPARE(model.data(model.index(0, 0), AssistantTaskTypeModel::IsChatRole).toBool(), true);
        QCOMPARE(model.data(model.index(1, 0), AssistantTaskTypeModel::NameRole).toString(), QStringLiteral("Summarize"));

        model.replaceFromResponse(ocsResponse(QJsonObject{}));
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.firstTypeId().isEmpty());
    }

    void tasksFilterFlattenAndMapStatuses()
    {
        AssistantTaskModel model;
        const auto type = QStringLiteral("core:text2text:summarize");
        const auto tasks = QJsonArray{
            task(1, type, QStringLiteral("STATUS_SCHEDULED")),
            task(2, type, QStringLiteral("STATUS_RUNNING")),
            task(3, type, QStringLiteral("STATUS_SUCCESSFUL")),
            task(4, type, QStringLiteral("STATUS_FAILED")),
            task(5, type, QStringLiteral("unexpected")),
            task(6, QStringLiteral("core:text2text:other"), QStringLiteral("STATUS_SUCCESSFUL")),
            task(7, type, QStringLiteral("STATUS_SUCCESSFUL"), QStringLiteral("other-app")),
        };

        model.replaceFromResponse(ocsResponse(QJsonObject{{QStringLiteral("tasks"), tasks}}), type);

        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(model.data(model.index(0, 0), AssistantTaskModel::InputRole).toString(), QStringLiteral("Question"));
        QCOMPARE(model.data(model.index(0, 0), AssistantTaskModel::OutputRole).toString(), QStringLiteral("Answer"));
        QCOMPARE(model.data(model.index(0, 0), AssistantTaskModel::StatusTextRole).toString(), QStringLiteral("Scheduled"));
        QCOMPARE(model.data(model.index(1, 0), AssistantTaskModel::StatusTextRole).toString(), QStringLiteral("In progress"));
        QCOMPARE(model.data(model.index(2, 0), AssistantTaskModel::StatusTextRole).toString(), QStringLiteral("Completed"));
        QCOMPARE(model.data(model.index(3, 0), AssistantTaskModel::StatusTextRole).toString(), QStringLiteral("Failed"));
        QCOMPARE(model.data(model.index(4, 0), AssistantTaskModel::StatusTextRole).toString(), QStringLiteral("Unknown"));
        QCOMPARE(model.inputForTask(3), QStringLiteral("Question"));
        QVERIFY(model.runningState(2).has_value());
        QCOMPARE(*model.runningState(2), true);
        QVERIFY(model.runningState(3).has_value());
        QCOMPARE(*model.runningState(3), false);
        QVERIFY(!model.runningState(99).has_value());

        model.replaceFromResponse(ocsResponse(QJsonObject{{QStringLiteral("task"), task(8, type, QStringLiteral("3"))}}), type);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, 0), AssistantTaskModel::TaskIdRole).toLongLong(), 8);
        QVERIFY(model.runningState(8).has_value());
        QCOMPARE(*model.runningState(8), false);

        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void conversationsSelectRenamePrependAndClear()
    {
        AssistantConversationModel model;
        model.replaceFromResponse(QJsonDocument{QJsonArray{
                                      conversation(41, QStringLiteral("First")),
                                      conversation(42),
                                  }},
                                  41);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), AssistantConversationModel::SelectedRole).toBool(), true);
        QVERIFY(!model.data(model.index(1, 0), AssistantConversationModel::TitleRole).toString().isEmpty());
        QVERIFY(model.contains(42));
        QCOMPARE(model.titleForConversation(41), QStringLiteral("First"));

        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
        model.select(42);
        QCOMPARE(model.data(model.index(0, 0), AssistantConversationModel::SelectedRole).toBool(), false);
        QCOMPARE(model.data(model.index(1, 0), AssistantConversationModel::SelectedRole).toBool(), true);
        QCOMPARE(changedSpy.count(), 2);

        changedSpy.clear();
        model.updateTitle(42, QStringLiteral("Renamed"));
        QCOMPARE(model.titleForConversation(42), QStringLiteral("Renamed"));
        QCOMPARE(changedSpy.count(), 1);

        model.prepend(conversation(43, QStringLiteral("Newest")), 43);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0, 0), AssistantConversationModel::ConversationIdRole).toLongLong(), 43);
        QCOMPARE(model.data(model.index(0, 0), AssistantConversationModel::SelectedRole).toBool(), true);

        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.contains(43));
    }

    void messagesNormalizeRolesAndSessionKeys()
    {
        AssistantMessageModel model;
        model.replaceFromResponse(QJsonDocument{QJsonArray{
            message(1, QStringLiteral("session_id"), 41, QStringLiteral("human"), QStringLiteral("Question")),
            message(2, QStringLiteral("sessionId"), 41, QStringLiteral("assistant"), QStringLiteral("Answer")),
        }});

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), AssistantMessageModel::MessageRole).toString(), QStringLiteral("user"));
        QCOMPARE(model.data(model.index(0, 0), AssistantMessageModel::SessionIdRole).toLongLong(), 41);
        QCOMPARE(model.data(model.index(1, 0), AssistantMessageModel::SessionIdRole).toLongLong(), 41);
        QCOMPARE(model.data(model.index(1, 0), AssistantMessageModel::TextRole).toString(), QStringLiteral("Answer"));
        QVERIFY(!model.data(model.index(1, 0), AssistantMessageModel::DateTextRole).toString().isEmpty());
        QVERIFY(!model.lastMessageIsHuman());

        model.append(message(3, QStringLiteral("sessionId"), 41, QStringLiteral("human"), QStringLiteral("Again")));
        QCOMPARE(model.rowCount(), 3);
        QVERIFY(model.lastMessageIsHuman());

        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.lastMessageIsHuman());
    }
};

QTEST_GUILESS_MAIN(TestAssistantModels)
#include "testassistantmodels.moc"

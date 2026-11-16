/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistant/assistantconversationmodel.h"
#include "assistant/assistantmessagemodel.h"
#include "assistant/assistanttaskmodel.h"
#include "assistant/assistanttasktypemodel.h"

#include <QAbstractItemModelTester>
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
    void emptyMessagesAreHidden_data()
    {
        QTest::addColumn<bool>("incremental");
        QTest::newRow("history") << false;
        QTest::newRow("append") << true;
    }

    void emptyMessagesAreHidden()
    {
        using namespace Qt::StringLiterals;
        QFETCH(bool, incremental);
        auto model = AssistantMessageModel{};
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto inserted = QSignalSpy{&model, &QAbstractItemModel::rowsInserted};
        auto attachmentOnly = message(6, u"sessionId"_s, 42, u"human"_s, {});
        attachmentOnly.insert(u"attachments"_s, QJsonArray{QJsonObject{{u"file_id"_s, 12}, {u"type"_s, u"File"_s}}});
        auto invalidAttachment = message(4, u"sessionId"_s, 42, u"assistant"_s, {});
        invalidAttachment.insert(u"attachments"_s, QJsonArray{QJsonObject{{u"file_id"_s, -1}}});
        const auto messages = QJsonArray{
            message(1, u"sessionId"_s, 42, u"human"_s, {}),
            message(2, u"sessionId"_s, 42, u"user"_s, {}),
            message(3, u"sessionId"_s, 42, u"assistant"_s, u" \n\t"_s),
            invalidAttachment,
            message(5, u"sessionId"_s, 42, u"assistant"_s, u"Answer"_s),
            attachmentOnly,
        };
        if (incremental) {
            for (const auto &entry : messages) {
                model.append(entry.toObject());
            }
            QCOMPARE(inserted.size(), 2);
        } else {
            model.replaceFromResponse(QJsonDocument{messages});
        }
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), AssistantMessageModel::MessageIdRole).toLongLong(), 5);
        QCOMPARE(model.data(model.index(1, 0), AssistantMessageModel::MessageIdRole).toLongLong(), 6);
        QVERIFY(model.data(model.index(1, 0), AssistantMessageModel::TextRole).toString().isEmpty());
        QCOMPARE(model.data(model.index(1, 0), AssistantMessageModel::AttachmentsRole).toList().size(), 1);
        QVERIFY(model.lastMessageIsHuman());

        model.replaceFromResponse(QJsonDocument{QJsonArray{messages.at(0), messages.at(1), messages.at(2), invalidAttachment}});
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.lastMessageIsHuman());
    }

    void messagesPreserveGenericAttachments()
    {
        using namespace Qt::StringLiterals;
        auto model = AssistantMessageModel{};
        const auto response = QJsonObject{
            {u"id"_s, 1},
            {u"role"_s, u"assistant"_s},
            {u"content"_s, u""_s},
            {u"ocp_task_id"_s, 42},
            {u"attachments"_s,
             QJsonArray{
                 QJsonObject{{u"type"_s, u"File"_s}, {u"file_id"_s, 11}},
                 QJsonObject{{u"type"_s, u"Image"_s}, {u"fileId"_s, u"12"_s}, {u"ocp_task_id"_s, 43}},
                 QJsonObject{{u"type"_s, u"Audio"_s}, {u"file_id"_s, 13}},
                 QJsonObject{{u"file_id"_s, -1}},
                 QJsonObject{{u"file_id"_s, u"invalid"_s}},
                 QJsonValue{},
             }},
        };
        model.replaceFromResponse(QJsonDocument{QJsonArray{response}});
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.data(model.index(0, 0), AssistantMessageModel::TextRole).toString().isEmpty());
        const auto attachments = model.data(model.index(0, 0), AssistantMessageModel::AttachmentsRole).toList();
        QCOMPARE(attachments.size(), 3);
        QCOMPARE(attachments[0].toMap().value(u"fileId"_s).toLongLong(), 11);
        QCOMPARE(attachments[0].toMap().value(u"taskId"_s).toLongLong(), 42);
        QCOMPARE(attachments[1].toMap().value(u"fileId"_s).toLongLong(), 12);
        QCOMPARE(attachments[1].toMap().value(u"taskId"_s).toLongLong(), 43);
        QCOMPARE(attachments[2].toMap().value(u"type"_s).toString(), u"Audio"_s);

        auto human = response;
        human.insert(u"role"_s, u"human"_s);
        model.append(human);
        QCOMPARE(model.data(model.index(1, 0), AssistantMessageModel::AttachmentsRole).toList()[1].toMap().value(u"taskId"_s).toLongLong(), -1);
        model.replaceFromResponse(QJsonDocument{QJsonArray{QJsonObject{{u"content"_s, u"No attachments"_s}}}});
        QVERIFY(model.data(model.index(0, 0), AssistantMessageModel::AttachmentsRole).toList().isEmpty());
    }

    void taskTypesFilterSortAndExposeMetadata()
    {
        AssistantTaskTypeModel model;
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
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
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
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
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
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

    void conversationsRemoveById()
    {
        using namespace Qt::StringLiterals;
        auto model = AssistantConversationModel{};
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.replaceFromResponse(QJsonDocument{QJsonArray{conversation(42, u"Current"_s), conversation(43, u"Other"_s)}}, 42);
        auto removed = QSignalSpy(&model, &QAbstractItemModel::rowsRemoved);
        model.remove(43);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(removed.size(), 1);
        QVERIFY(model.data(model.index(0, 0), AssistantConversationModel::SelectedRole).toBool());
        model.remove(43);
        QCOMPARE(removed.size(), 1);
        model.remove(42);
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.contains(42));
    }

    void messagesNormalizeRolesAndSessionKeys()
    {
        AssistantMessageModel model;
        const auto modelTester = QAbstractItemModelTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
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

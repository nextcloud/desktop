/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "gui/tray/activitydata.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "syncenginetestutils.h"
#include "syncfileitem.h"
#include "folder.h"
#include "folderman.h"
#include "testhelper.h"

#include <QTest>
#include <QHash>

namespace {
    constexpr auto events = 3;
    constexpr auto eventC = "event";
    constexpr auto calendarC = "calendar";
    constexpr auto nameC = "name";
    constexpr auto linkC = "link";
    constexpr auto typeC = "type";
    constexpr auto idC = "id";
};

class TestActivityData : public QObject
{
    Q_OBJECT

public:
    TestActivityData() = default;

    void createFilesActivityJsonData(QString fileFormat, QString mimeType)
    {
        const auto objectType = QStringLiteral("files");
        const auto subject = QStringLiteral("You created path/test.").append(fileFormat);
        const auto path = QStringLiteral("path/test.").append(fileFormat);
        const auto fileName = QStringLiteral("test.").append(fileFormat);
        const auto activityType = QStringLiteral("file");
        const auto activityId = 90000;
        const auto message = QString();
        const auto objectName = QStringLiteral("test.%1").arg(fileFormat);
        const auto link = account->url().toString().append(QStringLiteral("/f/%1").arg(activityId));
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(QStringLiteral("/apps/files/img/add-color.svg"));

        const QJsonObject richStringData({
            {typeC, activityType},
            {idC, activityId},
            {linkC,  link},
            {nameC, fileName},
            {QStringLiteral("path"), objectName}
        });

        const auto subjectRichString = QStringLiteral("You created {file1}");
        const auto subjectRichObj = QJsonObject({{QStringLiteral("file1"), richStringData}});
        const auto subjectRichData = QJsonArray({subjectRichString, subjectRichObj});

        const auto previewUrl = account->url().toString().append(QStringLiteral("/index.php/core/preview.png?file=/")).append(path);

        // Text file previews should be replaced by mimetype icon
        const QJsonObject previewData({
            {linkC, link},
            {QStringLiteral("mimeType"), mimeType},
            {QStringLiteral("fileId"), activityId},
            {QStringLiteral("filename"), fileName},
            {QStringLiteral("view"), QStringLiteral("files")},
            {QStringLiteral("source"), previewUrl},
            {QStringLiteral("isMimeTypeIcon"), false},
        });

        QJsonObject testData({
            {QStringLiteral("object_type"), objectType},
            {QStringLiteral("activity_id"), activityId},
            {typeC, activityType},
            {QStringLiteral("subject"), subject},
            {QStringLiteral("message"), message},
            {QStringLiteral("object_name"), objectName},
            {linkC, link},
            {QStringLiteral("datetime"), datetime},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("subject_rich"), subjectRichData},
            {QStringLiteral("previews"), QJsonArray({previewData})},
        });

        QTest::addRow("data") << testData << fileFormat << mimeType << objectType << subject << path << fileName << activityType << activityId << message << objectName << link << datetime << icon << subjectRichString << subjectRichData << previewUrl;
    }

    void createCalendarActivityJsonData(QString name, QString event, QString calendar)
    {
        const auto objectType = calendarC;
        const auto subject = QStringLiteral("%1 updated event %2 in calendar %3").arg(name, event, calendar);
        const auto activityType = QStringLiteral("calendar_event");
        const auto activityId = 10000;
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(QStringLiteral("/core/img/places/calendar.svg"));
        const auto eventLink = QStringLiteral("/apps/calendar/dayGridMonth/now/edit/sidebar/A12bcD12AbcDEFgH456/next");

        const auto subjectRichString = QStringLiteral("{actor} updated event {event} in calendar {calendar}");
        const QJsonArray subjectRichData({
            subjectRichString,
            {{
                {objectType, {{
                    {typeC, objectType},
                    {idC, QString()},
                    {linkC,  QString()},
                    {nameC, calendar},
                }}},
                {eventC, {{
                    {typeC, QStringLiteral("calendar-event")},
                    {idC, QStringLiteral("12AA3456-A1B2-A1B2-A1B2-AB12C34567D8")},
                    {linkC, account->url().toString().append(eventLink)},
                    {nameC, event},
                }}},
                {QStringLiteral("actor"), {{
                    {typeC, QStringLiteral("user")},
                    {idC, QStringLiteral("username")},
                    {linkC,  QString()},
                    {nameC, name},
                }}},
            }},
        });

        QJsonObject testData({
            {QStringLiteral("object_type"), objectType},
            {QStringLiteral("subject"), subject},
            {typeC, activityType},
            {QStringLiteral("activity_id"), activityId},
            {QStringLiteral("object_name"), QString()},
            {QStringLiteral("datetime"), datetime},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("subject_rich"), subjectRichData},
        });

        QTest::addRow("data") << testData << QString() << QString() << objectType << subject << QString() << QString() << activityType << activityId << QString() << QString() << QString() << datetime << icon << subjectRichString << subjectRichData << QString();
    }

    // We always "use" event. What you are observing comes from the event merging, e.g. if a user creates two events,
    // there will be only 1 activity entry saying "X created event A and B", but the mechanism is incremental and
    // when you only modified the same entry twice, you get "X updated event A" with A being event1 instead of event.
    void createCalendarEventMergedActivityJsonData(QString name, QString event, QString calendar, int mergedEvent)
    {
        const auto objectType = calendarC;
        const auto subject = QStringLiteral("%1 updated event %2 in calendar %3").arg(name, event, calendar);
        const auto activityType = QStringLiteral("calendar_event");
        const auto activityId = 10000;
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(QStringLiteral("/core/img/places/calendar.svg"));
        const auto eventLink = QStringLiteral("/apps/calendar/dayGridMonth/now/edit/sidebar/A12bcD12AbcDEFgH456/next");

        const auto subjectRichString = QStringLiteral("{actor} updated event {event%1} in calendar {calendar}").arg(mergedEvent);
        const QJsonArray subjectRichData({
            subjectRichString,
            {{
              {objectType, {{
                  {typeC, objectType},
                  {idC, QString()},
                  {linkC,  QString()},
                  {nameC, calendar},
              }}},
              {QStringLiteral("event%1").arg(mergedEvent), {{
                  {typeC, QStringLiteral("calendar-event")},
                  {idC, QStringLiteral("12AA3456-A1B2-A1B2-A1B2-AB12C34567D8")},
                  {linkC, account->url().toString().append(eventLink)},
                {nameC, event},
              }}},
              {QStringLiteral("actor"), {{
                  {typeC, QStringLiteral("user")},
                  {idC, QStringLiteral("username")},
                  {linkC,  QString()},
                  {nameC, name},
              }}},
           }},
        });

        QJsonObject testData({
            {QStringLiteral("object_type"), objectType},
            {QStringLiteral("subject"), subject},
            {typeC, activityType},
            {QStringLiteral("activity_id"), activityId},
            {QStringLiteral("object_name"), QString()},
            {QStringLiteral("datetime"), datetime},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("subject_rich"), subjectRichData},
        });

        QTest::addRow("data") << testData << QString() << QString() << objectType << subject << QString() << QString() << activityType << activityId << QString() << QString() << QString() << datetime << icon << subjectRichString << subjectRichData << QString();
    }

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));
        auto *cred = new HttpCredentialsTest("testuser", "secret");
        account->setCredentials(cred);
    }

    void testFromJson_data()
    {
        QTest::addColumn<QJsonObject>("activityJsonObject");
        QTest::addColumn<QString>("fileFormat");
        QTest::addColumn<QString>("mimeTypeExpected");
        QTest::addColumn<QString>("objectTypeExpected");
        QTest::addColumn<QString>("subjectExpected");
        QTest::addColumn<QString>("pathExpected");
        QTest::addColumn<QString>("fileNameExpected");
        QTest::addColumn<QString>("activityTypeExpected");
        QTest::addColumn<int>("activityIdExpected");
        QTest::addColumn<QString>("messageExpected");
        QTest::addColumn<QString>("objectNameExpected");
        QTest::addColumn<QString>("linkExpected");
        QTest::addColumn<QString>("datetimeExpected");
        QTest::addColumn<QString>("iconExpected");
        QTest::addColumn<QString>("subjectRichStringExpected");
        QTest::addColumn<QJsonArray>("subjectRichDataExpected");
        QTest::addColumn<QString>("previewUrlExpected");

        createFilesActivityJsonData(QStringLiteral("jpg"), QStringLiteral("image/jpg"));
        createFilesActivityJsonData(QStringLiteral("txt"), QStringLiteral("text/plain"));
        createFilesActivityJsonData(QStringLiteral("pdf"), QStringLiteral("application/pdf"));
        createCalendarActivityJsonData(account->displayName(), QStringLiteral("Event 1"), QStringLiteral("Calendar 1"));
        createCalendarActivityJsonData(account->displayName(), QStringLiteral("Event 2"), QStringLiteral("Calendar 2"));

        for (int i = 1; i <= events; i++) {
            createCalendarEventMergedActivityJsonData(account->displayName(), QStringLiteral("Event"), QStringLiteral("Calendar"), i);
        }
    }

    void testFromJson()
    {
        QFETCH(QJsonObject, activityJsonObject);
        QFETCH(QString, fileFormat);
        QFETCH(QString, mimeTypeExpected);
        QFETCH(QString, objectTypeExpected);
        QFETCH(QString, subjectExpected);
        QFETCH(QString, pathExpected);
        QFETCH(QString, fileNameExpected);
        QFETCH(QString, activityTypeExpected);
        QFETCH(int, activityIdExpected);
        QFETCH(QString, messageExpected);
        QFETCH(QString, objectNameExpected);
        QFETCH(QString, linkExpected);
        QFETCH(QString, datetimeExpected);
        QFETCH(QString, iconExpected);
        QFETCH(QString, subjectRichStringExpected);
        QFETCH(QJsonArray, subjectRichDataExpected);
        QFETCH(QString, previewUrlExpected);

        OCC::Activity activity = OCC::Activity::fromActivityJson(activityJsonObject, account);
        QCOMPARE(activity._type, OCC::Activity::ActivityType);
        QCOMPARE(activity._objectType, objectTypeExpected);
        QCOMPARE(activity._id, activityIdExpected);
        QCOMPARE(activity._fileAction, activityTypeExpected);
        QCOMPARE(activity._accName, account->displayName());
        QCOMPARE(activity._subject, subjectExpected);
        QCOMPARE(activity._message, messageExpected);
        QCOMPARE(activity._file, objectNameExpected);
        QCOMPARE(activity._dateTime, QDateTime::fromString(datetimeExpected, Qt::ISODate));
        QCOMPARE(activity._objectName, objectNameExpected);

        const auto richParams = activity._subjectRichParameters;

        if (objectTypeExpected != calendarC) {
            QCOMPARE(activity._link, linkExpected);

            QCOMPARE(richParams.count(), 1);
            QCOMPARE(activity._subjectDisplay, QStringLiteral("You created ").append(fileNameExpected));

            const auto previews = activity._previews;

            QCOMPARE(previews.count(), 1);

            auto icon = account->url().toString().append("/index.php/apps/theming/img/core/filetypes/%1.svg");
            const auto isMimeTypeIcon = true;

            if(fileFormat == "txt") {
                QCOMPARE(previews[0]._source, icon.arg("text"));
                QCOMPARE(previews[0]._isMimeTypeIcon, isMimeTypeIcon);
            } else if(fileFormat == "pdf") {
                QCOMPARE(previews[0]._source, icon.arg("application-pdf"));
                QCOMPARE(previews[0]._isMimeTypeIcon, isMimeTypeIcon);
            } else {
                QCOMPARE(previews[0]._source, previewUrlExpected);
                QCOMPARE(previews[0]._isMimeTypeIcon, !isMimeTypeIcon);
            }

            QCOMPARE(previews[0]._mimeType, mimeTypeExpected);
        } else {
            QCOMPARE(richParams.count(), 3);
            QCOMPARE(subjectRichDataExpected.count(), 2);

            QString eventName;
            QUrl eventLink;
            auto expectedParams = subjectRichDataExpected[1].toObject();
            for (auto i = expectedParams.begin(); i != expectedParams.end(); ++i) {
                if (i.key().startsWith(eventC)) {
                    const auto expectedJsonObject = i.value().toObject();
                    eventName = expectedJsonObject.value(nameC).toString();
                    eventLink = expectedJsonObject.value(linkC).toString();
                    break;
                }
            }

            QCOMPARE(activity._link, eventLink);
            QCOMPARE(activity._subjectDisplay, QStringLiteral("%1 updated event %2 in calendar %3").arg(account->displayName(),
                                                                                                        eventName,
                                                                                                        richParams[calendarC].value<OCC::Activity::RichSubjectParameter>().name));
            QCOMPARE(activity._icon, iconExpected);
        }
    }
};

QTEST_MAIN(TestActivityData)
#include "testactivitydata.moc"

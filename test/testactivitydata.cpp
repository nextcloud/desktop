/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/tray/activitydata.h"
#include "account.h"
#include "accountstate.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include <QTest>
#include <QHash>

using namespace Qt::StringLiterals;

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
        const auto objectType = u"files"_s;
        const auto subject = u"You created path/test."_s.append(fileFormat);
        const auto path = u"path/test."_s.append(fileFormat);
        const auto fileName = u"test."_s.append(fileFormat);
        const auto activityType = u"file"_s;
        const auto activityId = 90000;
        const auto message = QString();
        const auto objectName = u"test.%1"_s.arg(fileFormat);
        const auto link = account->url().toString().append(u"/f/%1"_s.arg(activityId));
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(u"/apps/files/img/add-color.svg"_s);

        const QJsonObject richStringData({
            {typeC, activityType},
            {idC, activityId},
            {linkC,  link},
            {nameC, fileName},
            {u"path"_s, objectName}
        });

        const auto subjectRichString = u"You created {file1}"_s;
        const auto subjectRichObj = QJsonObject({{u"file1"_s, richStringData}});
        const auto subjectRichData = QJsonArray({subjectRichString, subjectRichObj});

        const auto previewUrl = account->url().toString().append(u"/index.php/core/preview.png?file=/"_s).append(path);

        // Text file previews should be replaced by mimetype icon
        const QJsonObject previewData({
            {linkC, link},
            {u"mimeType"_s, mimeType},
            {u"fileId"_s, activityId},
            {u"filename"_s, fileName},
            {u"view"_s, u"files"_s},
            {u"source"_s, previewUrl},
            {u"isMimeTypeIcon"_s, false},
        });

        QJsonObject testData({
            {u"object_type"_s, objectType},
            {u"activity_id"_s, activityId},
            {typeC, activityType},
            {u"subject"_s, subject},
            {u"message"_s, message},
            {u"object_name"_s, objectName},
            {linkC, link},
            {u"datetime"_s, datetime},
            {u"icon"_s, icon},
            {u"subject_rich"_s, subjectRichData},
            {u"previews"_s, QJsonArray({previewData})},
        });

        QTest::addRow("data") << testData << fileFormat << mimeType << objectType << subject << path << fileName << activityType << activityId << message << objectName << link << datetime << icon << subjectRichString << subjectRichData << previewUrl;
    }

    void createCalendarActivityJsonData(QString name, QString event, QString calendar)
    {
        const auto objectType = calendarC;
        const auto subject = u"%1 updated event %2 in calendar %3"_s.arg(name, event, calendar);
        const auto activityType = u"calendar_event"_s;
        const auto activityId = 10000;
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(u"/core/img/places/calendar.svg"_s);
        const auto eventLink = u"/apps/calendar/dayGridMonth/now/edit/sidebar/A12bcD12AbcDEFgH456/next"_s;

        const auto subjectRichString = u"{actor} updated event {event} in calendar {calendar}"_s;
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
                    {typeC, u"calendar-event"_s},
                    {idC, u"12AA3456-A1B2-A1B2-A1B2-AB12C34567D8"_s},
                    {linkC, account->url().toString().append(eventLink)},
                    {nameC, event},
                }}},
                {u"actor"_s, {{
                    {typeC, u"user"_s},
                    {idC, u"username"_s},
                    {linkC,  QString()},
                    {nameC, name},
                }}},
            }},
        });

        QJsonObject testData({
            {u"object_type"_s, objectType},
            {u"subject"_s, subject},
            {typeC, activityType},
            {u"activity_id"_s, activityId},
            {u"object_name"_s, QString()},
            {u"datetime"_s, datetime},
            {u"icon"_s, icon},
            {u"subject_rich"_s, subjectRichData},
        });

        QTest::addRow("data") << testData << QString() << QString() << objectType << subject << QString() << QString() << activityType << activityId << QString() << QString() << QString() << datetime << icon << subjectRichString << subjectRichData << QString();
    }

    // We always "use" event. What you are observing comes from the event merging, e.g. if a user creates two events,
    // there will be only 1 activity entry saying "X created event A and B", but the mechanism is incremental and
    // when you only modified the same entry twice, you get "X updated event A" with A being event1 instead of event.
    void createCalendarEventMergedActivityJsonData(QString name, QString event, QString calendar, int mergedEvent)
    {
        const auto objectType = calendarC;
        const auto subject = u"%1 updated event %2 in calendar %3"_s.arg(name, event, calendar);
        const auto activityType = u"calendar_event"_s;
        const auto activityId = 10000;
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(u"/core/img/places/calendar.svg"_s);
        const auto eventLink = u"/apps/calendar/dayGridMonth/now/edit/sidebar/A12bcD12AbcDEFgH456/next"_s;

        const auto subjectRichString = u"{actor} updated event {event%1} in calendar {calendar}"_s.arg(mergedEvent);
        const QJsonArray subjectRichData({
            subjectRichString,
            {{
              {objectType, {{
                  {typeC, objectType},
                  {idC, QString()},
                  {linkC,  QString()},
                  {nameC, calendar},
              }}},
              {u"event%1"_s.arg(mergedEvent), {{
                  {typeC, u"calendar-event"_s},
                  {idC, u"12AA3456-A1B2-A1B2-A1B2-AB12C34567D8"_s},
                  {linkC, account->url().toString().append(eventLink)},
                {nameC, event},
              }}},
              {u"actor"_s, {{
                  {typeC, u"user"_s},
                  {idC, u"username"_s},
                  {linkC,  QString()},
                  {nameC, name},
              }}},
           }},
        });

        QJsonObject testData({
            {u"object_type"_s, objectType},
            {u"subject"_s, subject},
            {typeC, activityType},
            {u"activity_id"_s, activityId},
            {u"object_name"_s, QString()},
            {u"datetime"_s, datetime},
            {u"icon"_s, icon},
            {u"subject_rich"_s, subjectRichData},
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

        createFilesActivityJsonData(u"jpg"_s, u"image/jpg"_s);
        createFilesActivityJsonData(u"txt"_s, u"text/plain"_s);
        createFilesActivityJsonData(u"pdf"_s, u"application/pdf"_s);
        createCalendarActivityJsonData(account->displayName(), u"Event 1"_s, u"Calendar 1"_s);
        createCalendarActivityJsonData(account->displayName(), u"Event 2"_s, u"Calendar 2"_s);

        for (int i = 1; i <= events; i++) {
            createCalendarEventMergedActivityJsonData(account->displayName(), u"Event"_s, u"Calendar"_s, i);
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
            QCOMPARE(activity._subjectDisplay, u"You created "_s.append(fileNameExpected));

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
            QCOMPARE(activity._subjectDisplay, u"%1 updated event %2 in calendar %3"_s.arg(account->displayName(),
                                                                                           eventName,
                                                                                           richParams[calendarC].value<OCC::Activity::RichSubjectParameter>().name));
            QCOMPARE(activity._icon, iconExpected);
        }
    }

    void testRichParametersWithDashKeyeee()
    {
        QJsonParseError parseError;
        const auto activityJsonDocument = QJsonDocument::fromJson(
            // MOC struggles with multiline raw strings (R"()"), so this will have to do ...
            "{"
            "  \"activity_id\": 20891,"
            "  \"app\": \"tables\","
            "  \"type\": \"tables\","
            "  \"user\": \"Testuser\","
            "  \"subject\": \"You have updated cell How to do on row #42 in table Welcome to Nextcloud Tables!\","
            "  \"subject_rich\": ["
            "    \"You have updated cell {col-20} on row {row} in table {table}\","
            "    {"
            "      \"user\": {"
            "        \"type\": \"user\","
            "        \"id\": \"jyrki\","
            "        \"name\": \"Jyrki\""
            "      },"
            "      \"table\": {"
            "        \"type\": \"highlight\","
            "        \"id\": \"5\","
            "        \"name\": \"Welcome to Nextcloud Tables!\","
            "        \"link\": \"https://nextcloud.local/apps/tables/#/table/5\""
            "      },"
            "      \"row\": {"
            "        \"type\": \"highlight\","
            "        \"id\": \"42\","
            "        \"name\": \"#42\","
            "        \"link\": \"https://nextcloud.local/apps/tables/#/table/5/row/42\""
            "      },"
            "      \"col-20\": {"
            "        \"type\": \"highlight\","
            "        \"id\": \"20\","
            "        \"name\": \"How to do\""
            "      }"
            "    }"
            "  ],"
            "  \"message\": \"\","
            "  \"message_rich\": ["
            "    \"\","
            "    []"
            "  ],"
            "  \"object_type\": \"tables_row\","
            "  \"object_id\": 42,"
            "  \"object_name\": \"#42\","
            "  \"objects\": {"
            "    \"42\": \"#42\""
            "  },"
            "  \"link\": \"https://nextcloud.local/apps/tables/#/table/5/row/42\","
            "  \"icon\": \"https://nextcloud.local/apps/files/img/change.svg\","
            "  \"datetime\": \"2026-01-30T08:47:02+00:00\","
            "  \"previews\": []"
            "}"_ba, &parseError);

        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(activityJsonDocument.isObject());
        const auto activityJsonObject = activityJsonDocument.object();

        OCC::Activity activity = OCC::Activity::fromActivityJson(activityJsonObject, account);
        // GitHub #9327: the placeholder `{col-20}` was not replaced before.
        QCOMPARE(activity._subjectDisplay, "You have updated cell How to do on row #42 in table Welcome to Nextcloud Tables!"_L1);
    }
};

QTEST_MAIN(TestActivityData)
#include "testactivitydata.moc"

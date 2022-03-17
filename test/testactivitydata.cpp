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

class TestActivityData : public QObject
{
    Q_OBJECT

public:
    TestActivityData() = default;

    void createJsonSpecificFormatData(QString fileFormat, QString mimeType)
    {
        const auto objectType = QStringLiteral("files");
        const auto subject = QStringLiteral("You created path/test.").append(fileFormat);
        const auto path = QStringLiteral("path/test.").append(fileFormat);
        const auto fileName = QStringLiteral("test.").append(fileFormat);
        const auto activityType = QStringLiteral("file");
        const auto activityId = 90000;
        const auto message = QStringLiteral();
        const auto objectName = QStringLiteral("test.").append(fileFormat);
        const auto link = account->url().toString().append(QStringLiteral("/f/")).append(activityId);
        const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODate);
        const auto icon = account->url().toString().append(QStringLiteral("/apps/files/img/add-color.svg"));

        const QJsonObject richStringData({
            {QStringLiteral("type"), activityType},
            {QStringLiteral("id"), activityId},
            {QStringLiteral("link"),  link},
            {QStringLiteral("name"), fileName},
            {QStringLiteral("path"), objectName}
        });

        const auto subjectRichString = QStringLiteral("You created {file1}");
        const auto subjectRichObj = QJsonObject({{QStringLiteral("file1"), richStringData}});
        const auto subjectRichData = QJsonArray({subjectRichString, subjectRichObj});

        const auto previewUrl = account->url().toString().append(QStringLiteral("/index.php/core/preview.png?file=/")).append(path);

        // Text file previews should be replaced by mimetype icon
        const QJsonObject previewData({
            {QStringLiteral("link"), link},
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
            {QStringLiteral("type"), activityType},
            {QStringLiteral("subject"), subject},
            {QStringLiteral("message"), message},
            {QStringLiteral("object_name"), objectName},
            {QStringLiteral("link"), link},
            {QStringLiteral("datetime"), datetime},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("subject_rich"), subjectRichData},
            {QStringLiteral("previews"), QJsonArray({previewData})},
        });

        QTest::addRow("data") << testData << fileFormat << mimeType << objectType << subject << path << fileName << activityType << activityId << message << objectName << link << datetime << icon << subjectRichString << subjectRichData << previewUrl;
    }

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;

private slots:
    void initTestCase()
    {
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

        createJsonSpecificFormatData(QStringLiteral("jpg"), QStringLiteral("image/jpg"));
        createJsonSpecificFormatData(QStringLiteral("txt"), QStringLiteral("text/plain"));
        createJsonSpecificFormatData(QStringLiteral("pdf"), QStringLiteral("application/pdf"));
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
        QCOMPARE(activity._link, linkExpected);
        QCOMPARE(activity._dateTime, QDateTime::fromString(datetimeExpected, Qt::ISODate));

        QCOMPARE(activity._subjectRichParameters.count(), 1);
        QCOMPARE(activity._subjectDisplay, QStringLiteral("You created ").append(fileNameExpected));

        QCOMPARE(activity._previews.count(), 1);
        // We want the different icon when we have a preview
        //QCOMPARE(activity._icon, iconExpected);

        if(fileFormat == "txt") {
            QCOMPARE(activity._previews[0]._source, account->url().toString().append(QStringLiteral("/index.php/apps/theming/img/core/filetypes/text.svg")));
            QCOMPARE(activity._previews[0]._isMimeTypeIcon, true);
            QCOMPARE(activity._previews[0]._mimeType, mimeTypeExpected);
        } else if(fileFormat == "pdf") {
            QCOMPARE(activity._previews[0]._source, account->url().toString().append(QStringLiteral("/index.php/apps/theming/img/core/filetypes/application-pdf.svg")));
            QCOMPARE(activity._previews[0]._isMimeTypeIcon, true);
        } else {
            QCOMPARE(activity._previews[0]._source, previewUrlExpected);
            QCOMPARE(activity._previews[0]._isMimeTypeIcon, false);
        }

        QCOMPARE(activity._previews[0]._mimeType, mimeTypeExpected);
    }
};

QTEST_MAIN(TestActivityData)
#include "testactivitydata.moc"

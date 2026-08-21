/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/accountmanager.h"
#include "gui/filedetails/filedetails.h"
#include "gui/sharing/propertymodel.h"
#include "gui/sharing/sharingcontroller.h"
#include "gui/sharing/unifiedshare.h"
#include "gui/systray.h"
#include "gui/tray/usermodel.h"
#include "theme.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>

#include <memory>

using namespace OCC;
using namespace OCC::Gui::Sharing;

class TestSharingDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserModel", UserModel::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Theme", Theme::instance());
        qmlRegisterType<FileDetails>("com.nextcloud.desktopclient", 1, 0, "FileDetails");

        Systray::instance()->setTrayEngine(new QQmlApplicationEngine(QCoreApplication::instance()));
    }

    void createsDialogThroughSystrayComponentPath()
    {
        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));

        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("ShareDialog"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const QVariantMap initialProperties{
            {QStringLiteral("account"), QVariant::fromValue(account)},
            {QStringLiteral("localPath"), QString()},
            {QStringLiteral("fileId"), QString()},
            {QStringLiteral("remotePath"), QStringLiteral("example.txt")},
        };

        const auto dialogObject = std::unique_ptr<QObject>(component.createWithInitialProperties(initialProperties));
        QVERIFY2(dialogObject, qPrintable(component.errorString()));

        const auto dialog = qobject_cast<QQuickWindow *>(dialogObject.get());
        QVERIFY(dialog);

        const auto controller = dialogObject->property("controller").value<QObject *>();
        QVERIFY(controller);

        const auto selectedShare = dialogObject->property("selectedShare").value<Share *>();
        QVERIFY(!selectedShare);
        QVERIFY(!dialogObject->property("hasSelectedShare").toBool());

        const auto shareListPage = dialogObject->findChild<QObject *>(QStringLiteral("shareListPage"));
        QVERIFY(shareListPage);
        QCOMPARE(shareListPage->property("sharingController").value<QObject *>(), controller);

        const auto shareStackLayout = dialogObject->findChild<QObject *>(QStringLiteral("shareStackLayout"));
        QVERIFY(shareStackLayout);
        QCOMPARE(shareStackLayout->property("currentIndex").toInt(), 0);

        const auto shareListModel = dialogObject->findChild<QAbstractItemModel *>(QStringLiteral("shareListModel"));
        QVERIFY(shareListModel);
        QCOMPARE(shareListModel->rowCount(), 5);

        const auto shareDetailsFrame = dialogObject->findChild<QObject *>(QStringLiteral("shareDetailsFrame"));
        QVERIFY(shareDetailsFrame);
        QCOMPARE(shareDetailsFrame->property("sharingController").value<QObject *>(), controller);
        QVERIFY(!shareDetailsFrame->property("share").value<Share *>());

        // sharesChanged is emitted after the initial GET request completes. It
        // must not turn the empty selection into the details page.
        QVERIFY(QMetaObject::invokeMethod(controller, "sharesChanged", Qt::DirectConnection));
        QVERIFY(!dialogObject->property("selectedShare").value<Share *>());
        QVERIFY(!dialogObject->property("hasSelectedShare").toBool());
        QCOMPARE(shareStackLayout->property("currentIndex").toInt(), 0);

        const auto share = std::unique_ptr<Share>(Share::fromJson(QJsonDocument{QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("share-1")},
                    {QStringLiteral("state"), QStringLiteral("active")},
                    {QStringLiteral("recipients"), QJsonArray{QJsonObject{
                        {QStringLiteral("class"), QStringLiteral("OC\\Core\\Sharing\\Recipient\\UserShareRecipientType")},
                        {QStringLiteral("display_name"), QStringLiteral("admin")},
                        {QStringLiteral("value"), QStringLiteral("admin")},
                    }}},
                }},
            }},
        }}, account));
        QVERIFY(share);

        QVERIFY(dialogObject->setProperty("selectedShare", QVariant::fromValue(share.get())));
        QVERIFY(dialogObject->setProperty("hasSelectedShare", true));
        QCOMPARE(shareStackLayout->property("currentIndex").toInt(), 1);
        const auto backButton = dialogObject->findChild<QObject *>(QStringLiteral("backToShareListButton"));
        QVERIFY(backButton);
        QVERIFY(backButton->property("visible").toBool());

        QVERIFY(dialogObject->setProperty("hasSelectedShare", false));
        QCOMPARE(shareStackLayout->property("currentIndex").toInt(), 0);

        dialog->close();
    }

    void createsActiveShareDetailsWithRecipientControls()
    {
        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));

        const auto shareJson = QJsonDocument{QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("share-1")},
                    {QStringLiteral("state"), QStringLiteral("active")},
                    {QStringLiteral("recipients"), QJsonArray{QJsonObject{
                        {QStringLiteral("class"), QStringLiteral("OC\\Core\\Sharing\\Recipient\\UserShareRecipientType")},
                        {QStringLiteral("display_name"), QStringLiteral("admin")},
                        {QStringLiteral("value"), QStringLiteral("admin")},
                    }}},
                }},
            }},
        }};
        const auto share = std::unique_ptr<Share>(Share::fromJson(shareJson, account));
        QVERIFY(share);

        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("ShareDetailsFrame"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const auto frameObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("account"), QVariant::fromValue(account)},
            {QStringLiteral("share"), QVariant::fromValue(share.get())},
            {QStringLiteral("width"), 800},
            {QStringLiteral("height"), 600},
        }));
        QVERIFY2(frameObject, qPrintable(component.errorString()));

        const auto detailsPage = frameObject->findChild<QObject *>(QStringLiteral("shareDetailsPage"));
        QVERIFY(detailsPage);
        QCOMPARE(detailsPage->property("share").value<Share *>(), share.get());
        QCOMPARE(detailsPage->property("account").value<AccountPtr>(), account);
        QVERIFY(detailsPage->property("shareIsActive").toBool());

        const auto recipientSearch = detailsPage->findChild<QObject *>(QStringLiteral("recipientSearch"));
        QVERIFY(recipientSearch);
        QVERIFY(recipientSearch->property("visible").toBool());
        QCOMPARE(recipientSearch->property("account").value<AccountPtr>(), account);
        QCOMPARE(recipientSearch->property("shareId").toString(), QStringLiteral("share-1"));

        const auto recipientModel = detailsPage->findChild<QAbstractItemModel *>(QStringLiteral("recipientModel"));
        QVERIFY(recipientModel);
        QCOMPARE(recipientModel->rowCount(), 1);

        const auto deleteButton = frameObject->findChild<QObject *>(QStringLiteral("deleteShareButton"));
        const auto closeButton = frameObject->findChild<QObject *>(QStringLiteral("closeShareButton"));
        QVERIFY(deleteButton);
        QVERIFY(closeButton);
        QVERIFY(deleteButton->property("visible").toBool());
        QVERIFY(closeButton->property("visible").toBool());

        // Recipient editing is available while a draft is being composed as
        // well as after activation; activation must not remove the search or
        // recipient list from the editing page.
        share->updateFromJson(QJsonDocument{QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("state"), QStringLiteral("draft")},
                }},
            }},
        }});
        QCoreApplication::processEvents();
        QVERIFY(!detailsPage->property("shareIsActive").toBool());
        QVERIFY(recipientSearch->property("visible").toBool());
    }

    void createsOptionalPropertyFieldWithDisabledToggle()
    {
        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("FieldDelegate"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const QVariantMap model{
            {QStringLiteral("type"), PropertyModel::String},
            {QStringLiteral("label"), QStringLiteral("Expiration date")},
            {QStringLiteral("property"), QStringLiteral("expiration")},
            {QStringLiteral("advanced"), true},
            {QStringLiteral("required"), false},
            {QStringLiteral("value"), QString()},
        };
        const auto fieldObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("model"), model},
            {QStringLiteral("width"), 400},
        }));
        QVERIFY2(fieldObject, qPrintable(component.errorString()));

        const auto fieldItem = fieldObject->property("item").value<QObject *>();
        QVERIFY(fieldItem);
        const auto toggle = fieldItem->findChild<QObject *>(QStringLiteral("optionalFieldSwitch"));
        QVERIFY(toggle);
        QVERIFY(toggle->property("visible").toBool());
        QVERIFY(!toggle->property("checked").toBool());

        const auto fieldControl = fieldItem->findChild<QObject *>(QStringLiteral("optionalFieldControl"));
        QVERIFY(fieldControl);
        QVERIFY(!fieldControl->property("enabled").toBool());
    }

    void createsOptionalPropertyFieldWithEnabledToggle()
    {
        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("FieldDelegate"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const QVariantMap model{
            {QStringLiteral("type"), PropertyModel::String},
            {QStringLiteral("label"), QStringLiteral("Expiration date")},
            {QStringLiteral("property"), QStringLiteral("expiration")},
            {QStringLiteral("advanced"), true},
            {QStringLiteral("required"), false},
            {QStringLiteral("value"), QStringLiteral("2026-08-21")},
        };
        const auto fieldObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("model"), model},
            {QStringLiteral("width"), 400},
        }));
        QVERIFY2(fieldObject, qPrintable(component.errorString()));

        const auto fieldItem = fieldObject->property("item").value<QObject *>();
        QVERIFY(fieldItem);
        const auto toggle = fieldItem->findChild<QObject *>(QStringLiteral("optionalFieldSwitch"));
        QVERIFY(toggle);
        QVERIFY(toggle->property("visible").toBool());
        QVERIFY(toggle->property("checked").toBool());

        const auto fieldControl = fieldItem->findChild<QObject *>(QStringLiteral("optionalFieldControl"));
        QVERIFY(fieldControl);
        QVERIFY(fieldControl->property("enabled").toBool());
    }
};

QTEST_MAIN(TestSharingDialog)
#include "testsharingdialog.moc"

/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/accountmanager.h"
#include "gui/filedetails/filedetails.h"
#include "gui/sharing/propertymodel.h"
#include "gui/sharing/sharingcontroller.h"
#include "gui/systray.h"
#include "gui/tray/usermodel.h"
#include "theme.h"

#include <QGuiApplication>
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

        const auto shareListPage = dialogObject->findChild<QObject *>(QStringLiteral("shareListPage"));
        QVERIFY(shareListPage);
        QCOMPARE(shareListPage->property("sharingController").value<QObject *>(), controller);

        const auto shareDetailsFrame = dialogObject->findChild<QObject *>(QStringLiteral("shareDetailsFrame"));
        QVERIFY(shareDetailsFrame);
        QCOMPARE(shareDetailsFrame->property("sharingController").value<QObject *>(), controller);

        dialog->close();
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

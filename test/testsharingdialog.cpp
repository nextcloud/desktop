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

#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
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
        QCOMPARE(shareListModel->rowCount(), 2);

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
                    {QStringLiteral("properties"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("class"), QStringLiteral("OC\\Core\\Sharing\\Property\\NoteProperty")},
                            {QStringLiteral("display_name"), QStringLiteral("Note to recipients")},
                            {QStringLiteral("type"), QStringLiteral("string")},
                            {QStringLiteral("advanced"), false},
                            {QStringLiteral("required"), false},
                            {QStringLiteral("value"), QStringLiteral("Original note")},
                        },
                        QJsonObject{
                            {QStringLiteral("class"), QStringLiteral("OC\\Core\\Sharing\\Property\\ExpirationProperty")},
                            {QStringLiteral("display_name"), QStringLiteral("Expiration date")},
                            {QStringLiteral("type"), QStringLiteral("date")},
                            {QStringLiteral("advanced"), true},
                            {QStringLiteral("required"), false},
                            {QStringLiteral("value"), QString()},
                        },
                    }},
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
        QCoreApplication::processEvents();
        const auto scrollView = dialogObject->findChild<QQuickItem *>(QStringLiteral("shareDetailsScrollView"));
        const auto scrollBar = dialogObject->findChild<QQuickItem *>(QStringLiteral("shareDetailsScrollBar"));
        QVERIFY(scrollView);
        QVERIFY(scrollBar);
        QVERIFY(scrollView->width() > 0);
        QVERIFY(scrollView->height() > 0);
        QVERIFY(scrollBar->width() > 0);
        QVERIFY(scrollBar->height() >= scrollView->height() - 1);
        QVERIFY(scrollBar->x() > 0);
        QVERIFY(scrollBar->x() + scrollBar->width() <= scrollView->width() + 1);
        const auto backButton = dialogObject->findChild<QObject *>(QStringLiteral("backToShareListButton"));
        QVERIFY(backButton);
        QVERIFY(backButton->property("visible").toBool());

        const auto gearButton = dialogObject->findChild<QObject *>(QStringLiteral("advancedSettingsButton"));
        QVERIFY(gearButton);
        QVERIFY(gearButton->property("visible").toBool());
        QCOMPARE(dialogObject->property("advancedSettingsVisible").toBool(), false);

        QVERIFY(dialogObject->setProperty("advancedSettingsVisible", true));
        QCoreApplication::processEvents();
        const auto advancedPage = dialogObject->findChild<QObject *>(QStringLiteral("shareAdvancedSettingsPage"));
        QVERIFY(advancedPage);
        const auto shareDetailsLoader = dialogObject->findChild<QObject *>(QStringLiteral("shareDetailsLoader"));
        QVERIFY(shareDetailsLoader);
        QCOMPARE(shareDetailsLoader->property("item").value<QObject *>()->objectName(), QStringLiteral("shareAdvancedSettingsPage"));
        const auto advancedPropertyModel = advancedPage->findChild<QAbstractItemModel *>(QStringLiteral("advancedPropertyModel"));
        QVERIFY(advancedPropertyModel);
        QCOMPARE(advancedPropertyModel->rowCount(), 1);
        const auto basicPage = dialogObject->findChild<QObject *>(QStringLiteral("shareDetailsPage"));
        QVERIFY(basicPage);
        QVERIFY(!basicPage->property("visible").toBool());
        QCOMPARE(dialogObject->findChild<QObject *>(QStringLiteral("shareDialogTitle"))->property("text").toString(), QStringLiteral("Sharing settings"));
        QCOMPARE(dialogObject->findChild<QObject *>(QStringLiteral("shareDialogSubtitle"))->property("text").toString(), QStringLiteral("File"));

        QVERIFY(dialogObject->setProperty("advancedSettingsVisible", false));
        QCoreApplication::processEvents();
        QVERIFY(dialogObject->findChild<QObject *>(QStringLiteral("shareDetailsPage")));
        QCOMPARE(shareDetailsLoader->property("item").value<QObject *>()->objectName(), QStringLiteral("shareDetailsPage"));

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
        const auto cancelButton = frameObject->findChild<QObject *>(QStringLiteral("cancelShareButton"));
        const auto saveButton = frameObject->findChild<QObject *>(QStringLiteral("saveShareButton"));
        QVERIFY(deleteButton);
        QVERIFY(closeButton);
        QVERIFY(cancelButton);
        QVERIFY(saveButton);
        QVERIFY(!frameObject->property("footerVisible").toBool());
        QVERIFY(!deleteButton->property("visible").toBool());
        QVERIFY(!closeButton->property("visible").toBool());
        QVERIFY(!cancelButton->property("visible").toBool());
        QVERIFY(!saveButton->property("visible").toBool());

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
        QVERIFY(frameObject->property("footerVisible").toBool());
        QVERIFY(cancelButton->property("visible").toBool());
        QVERIFY(saveButton->property("visible").toBool());
    }

    void createsShareRowWithDeleteAction()
    {
        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));

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

        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("ShareRow"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const auto rowObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("share"), QVariant::fromValue(share.get())},
            {QStringLiteral("recipientNames"), QStringLiteral("admin")},
            {QStringLiteral("width"), 800},
        }));
        QVERIFY2(rowObject, qPrintable(component.errorString()));

        const auto deleteButton = rowObject->findChild<QObject *>(QStringLiteral("deleteShareRowButton"));
        QVERIFY(deleteButton);
        QVERIFY(deleteButton->property("visible").toBool());
        QVERIFY(deleteButton->property("enabled").toBool());
        QVERIFY(deleteButton->property("iconSource").toString().contains(QStringLiteral("delete.svg")));

        const auto hasDeleteSignal = [rowObject = rowObject.get()] {
            for (int methodIndex = 0; methodIndex < rowObject->metaObject()->methodCount(); ++methodIndex) {
                const auto method = rowObject->metaObject()->method(methodIndex);
                if (method.methodType() == QMetaMethod::Signal && method.name() == QByteArrayLiteral("deleteRequested")) {
                    return true;
                }
            }
            return false;
        }();
        QVERIFY(hasDeleteSignal);
    }

    void hidesFooterForActivePublicLinkDetails()
    {
        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));

        const auto share = std::unique_ptr<Share>(Share::fromJson(QJsonDocument{QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("public-share-1")},
                    {QStringLiteral("state"), QStringLiteral("active")},
                    {QStringLiteral("recipients"), QJsonArray{QJsonObject{
                        {QStringLiteral("class"), QStringLiteral("OC\\Core\\Sharing\\Recipient\\TokenShareRecipientType")},
                        {QStringLiteral("display_name"), QStringLiteral("Share link")},
                        {QStringLiteral("value"), QStringLiteral("token")},
                        {QStringLiteral("secret"), QJsonObject{{QStringLiteral("url"), QStringLiteral("https://cloud.example/s/token")}}},
                    }}},
                }},
            }},
        }}, account));
        QVERIFY(share);
        QVERIFY(share->isPublicLink());

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
        QVERIFY(!frameObject->property("footerVisible").toBool());
        const auto deleteButton = frameObject->findChild<QObject *>(QStringLiteral("deleteShareButton"));
        const auto closeButton = frameObject->findChild<QObject *>(QStringLiteral("closeShareButton"));
        QVERIFY(deleteButton);
        QVERIFY(closeButton);
        QVERIFY(!deleteButton->property("visible").toBool());
        QVERIFY(!closeButton->property("visible").toBool());
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
            {QStringLiteral("placeholder"), QStringLiteral("YYYY-MM-DD")},
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

    void createsMultilineNoteField()
    {
        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("FieldDelegate"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const QVariantMap model{
            {QStringLiteral("type"), PropertyModel::String},
            {QStringLiteral("label"), QStringLiteral("Note to recipients")},
            {QStringLiteral("property"), QStringLiteral("note-property")},
            {QStringLiteral("required"), false},
            {QStringLiteral("value"), QString()},
            {QStringLiteral("placeholder"), QStringLiteral("Note to recipients")},
        };
        const auto fieldObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("model"), model},
            {QStringLiteral("width"), 400},
        }));
        QVERIFY2(fieldObject, qPrintable(component.errorString()));

        const auto fieldItem = fieldObject->property("item").value<QObject *>();
        QVERIFY(fieldItem);
        const auto fieldControl = fieldItem->findChild<QObject *>(QStringLiteral("multilineFieldControl"));
        QVERIFY(fieldControl);
    }

    void preservesProvidedRecipientAvatar()
    {
        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QStringLiteral("com.nextcloud.desktopclient.sharing"),
            QStringLiteral("RecipientAvatar"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));

        const auto avatarObject = std::unique_ptr<QObject>(component.createWithInitialProperties({
            {QStringLiteral("source"), QUrl(QStringLiteral("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'><rect width='16' height='16' fill='red'/></svg>"))},
            {QStringLiteral("width"), 32},
            {QStringLiteral("height"), 32},
        }));
        QVERIFY2(avatarObject, qPrintable(component.errorString()));

        const auto image = avatarObject->findChild<QObject *>(QStringLiteral("recipientAvatarImage"));
        QVERIFY(image);
        QTRY_COMPARE(image->property("status").toInt(), 1);
        const auto effect = avatarObject->findChild<QObject *>(QStringLiteral("recipientAvatarEffect"));
        QVERIFY(effect);
        QVERIFY(effect->property("visible").toBool());
        QVERIFY(effect->property("maskEnabled").toBool());
        const auto mask = avatarObject->findChild<QObject *>(QStringLiteral("recipientAvatarMaskShape"));
        QVERIFY(mask);
        QCOMPARE(effect->property("maskSource").value<QObject *>(), mask);
        QCOMPARE(effect->property("source").value<QObject *>(), image);
    }

    void backIconResourceIsAvailable()
    {
        QVERIFY(QFile::exists(QStringLiteral(":/client/theme/back.svg")));
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
            {QStringLiteral("placeholder"), QStringLiteral("YYYY-MM-DD")},
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

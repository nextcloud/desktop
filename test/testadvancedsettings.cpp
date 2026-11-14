/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "advancedsettings.h"

#include "configfile.h"

#include <KZip>

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace OCC
{

class AdvancedSettingsTestAccess
{
public:
    static void setDebugArchiveLocationPicker(AdvancedSettings &settings, AdvancedSettings::DebugArchiveLocationPicker picker)
    {
        settings._debugArchiveLocationPicker = std::move(picker);
    }
};

}

using namespace OCC;

class TestAdvancedSettings : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDir;

    // Closes the next modal dialog once the event loop runs, so a message box cannot block the test.
    void closeNextModalDialog(bool &dialogShown)
    {
        QMetaObject::invokeMethod(
            this,
            [&dialogShown] {
                if (const auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                    dialogShown = true;
                    dialog->accept();
                }
            },
            Qt::QueuedConnection);
    }

    static void clickCreateDebugArchive(AdvancedSettings &settings)
    {
        const auto button = settings.findChild<QAbstractButton *>(u"debugArchiveButton"_s);
        QVERIFY(button);
        button->click();
    }

private Q_SLOTS:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QVERIFY(_configDir.isValid());
        ConfigFile::setConfDir(_configDir.path());
    }

    void chosenLocationReceivesTheArchive()
    {
        QTemporaryDir destinationDir;
        QVERIFY(destinationDir.isValid());
        const auto destination = destinationDir.filePath(u"debug.zip"_s);

        AdvancedSettings settings;
        AdvancedSettingsTestAccess::setDebugArchiveLocationPicker(settings, [&destination](const auto &completion) {
            completion(destination);
        });

        auto confirmationShown = false;
        closeNextModalDialog(confirmationShown);
        clickCreateDebugArchive(settings);

        QVERIFY(confirmationShown);
        KZip zip(destination);
        QVERIFY(zip.open(QIODevice::ReadOnly));
        QVERIFY(zip.directory()->entry(u"_client_parameters.txt"_s));
        QVERIFY(zip.directory()->entry(u"_client_buildinfo.txt"_s));
    }

    void cancellingCreatesNoArchive()
    {
        QTemporaryDir destinationDir;
        QVERIFY(destinationDir.isValid());

        AdvancedSettings settings;
        AdvancedSettingsTestAccess::setDebugArchiveLocationPicker(settings, [](const auto &completion) {
            completion({});
        });

        auto dialogShown = false;
        closeNextModalDialog(dialogShown);
        clickCreateDebugArchive(settings);
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

        QVERIFY(!dialogShown);
        QVERIFY(QDir(destinationDir.path()).isEmpty());
    }

    void completionAfterSettingsAreGoneIsIgnored()
    {
        QTemporaryDir destinationDir;
        QVERIFY(destinationDir.isValid());
        const auto destination = destinationDir.filePath(u"debug.zip"_s);

        std::function<void(const QString &)> pendingCompletion;
        {
            AdvancedSettings settings;
            AdvancedSettingsTestAccess::setDebugArchiveLocationPicker(settings, [&pendingCompletion](auto completion) {
                pendingCompletion = std::move(completion);
            });
            clickCreateDebugArchive(settings);
        }
        QVERIFY(pendingCompletion);

        auto dialogShown = false;
        closeNextModalDialog(dialogShown);
        pendingCompletion(destination);
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

        QVERIFY(!dialogShown);
        QVERIFY(!QFile::exists(destination));
    }
};

QTEST_MAIN(TestAdvancedSettings)
#include "testadvancedsettings.moc"

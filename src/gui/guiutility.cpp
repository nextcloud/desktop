/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "guiutility.h"

#include "config.h"

#include <QClipboard>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QUrlQuery>

#include "common/asserts.h"
#include "common/vfs.h"
using namespace OCC;

Q_LOGGING_CATEGORY(lcUtility, "nextcloud.gui.utility", QtInfoMsg)

bool Utility::openBrowser(const QUrl &url, QWidget *errorWidgetParent)
{
    if (!url.isValid()) {
        qCWarning(lcUtility) << "URL format is invalid and has been rejected:" << url.toString();
        return false;
    }

    const QStringList allowedUrlSchemes = {
        "http",
        "https",
    };
    
    const auto scheme = url.scheme().toLower();
    if (!allowedUrlSchemes.contains(scheme)) {
        qCWarning(lcUtility) << "URL scheme is not allowed and has been rejected:" << url.toString();
        return false;
    }

    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open browser"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the browser to go to "
                    "URL %1. Maybe no default browser is configured?")
                    .arg(url.toString()));
        }
        qCWarning(lcUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

bool Utility::openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent)
{
    QUrl url(QLatin1String("mailto:"));
    QUrlQuery query;
    query.setQueryItems({ { QLatin1String("subject"), subject },
        { QLatin1String("body"), body } });
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open email client"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the email client to "
                    "create a new message. Maybe no default email client is "
                    "configured?"));
        }
        qCWarning(lcUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

QString Utility::vfsCurrentAvailabilityText(VfsItemAvailability availability)
{
    switch(availability) {
    case VfsItemAvailability::AlwaysLocal:
        return QCoreApplication::translate("utility", "Always available locally");
    case VfsItemAvailability::AllHydrated:
        return QCoreApplication::translate("utility", "Currently available locally");
    case VfsItemAvailability::Mixed:
        return QCoreApplication::translate("utility", "Some available online only");
    case VfsItemAvailability::AllDehydrated:
    case VfsItemAvailability::OnlineOnly:
        return QCoreApplication::translate("utility", "Available online only");
    }
    Q_UNREACHABLE();
}

QString Utility::vfsPinActionText()
{
    return QCoreApplication::translate("utility", "Make always available locally");
}

QString Utility::vfsFreeSpaceActionText()
{
    return QCoreApplication::translate("utility", "Free up local space");
}

void Utility::askExperimentalVirtualFilesFeature(QWidget *receiver, const std::function<void(bool enable)> &callback)
{
#ifdef BUILD_FILE_PROVIDER_MODULE
    callback(true);
    return;
#endif

    const auto bestVfsMode = bestAvailableVfsMode();
    QMessageBox *msgBox = nullptr;
    QPushButton *acceptButton = nullptr;
    switch (bestVfsMode) {
    case Vfs::WindowsCfApi:
        callback(true);
        return;
    case Vfs::WithSuffix:
        msgBox = new QMessageBox(
            QMessageBox::Warning,
            QCoreApplication::translate("utility", "Enable experimental feature?"),
            QCoreApplication::translate("utility",
                "When the \"virtual files\" mode is enabled no files will be downloaded initially. "
                "Instead, a tiny \"%1\" file will be created for each file that exists on the server. "
                "The contents can be downloaded by running these files or by using their context menu."
                "\n\n"
                "The virtual files mode is mutually exclusive with selective sync. "
                "Currently unselected folders will be translated to online-only folders "
                "and your selective sync settings will be reset."
                "\n\n"
                "Switching to this mode will abort any currently running synchronization."
                "\n\n"
                "This is a new, experimental mode. If you decide to use it, please report any "
                "issues that come up.")
                .arg(APPLICATION_DOTVIRTUALFILE_SUFFIX),
            QMessageBox::NoButton, receiver);
        acceptButton = msgBox->addButton(QCoreApplication::translate("utility", "Enable experimental placeholder mode"), QMessageBox::AcceptRole);
        msgBox->addButton(QCoreApplication::translate("utility", "Stay safe"), QMessageBox::RejectRole);
        break;
    case Vfs::XAttr:
    case Vfs::Off:
        Q_UNREACHABLE();
    default:
        // Unhandled VFS mode - don't show the dialog
        callback(false);
        return;
    }

    QObject::connect(msgBox, &QMessageBox::accepted, receiver, [callback, msgBox, acceptButton] {
        callback(msgBox->clickedButton() == acceptButton);
        msgBox->deleteLater();
    });
    QObject::connect(msgBox, &QMessageBox::rejected, receiver, [callback, msgBox] {
        callback(false);
        msgBox->deleteLater();
    });
    msgBox->open();
}

void Utility::initErrorLabel(QLabel *errorLabel)
{
    const auto style = QLatin1String("border: 1px solid #eed3d7; border-radius: 5px; padding: 3px;"
                                    "background-color: #f2dede; color: #b94a48;");

    errorLabel->setStyleSheet(style);
    errorLabel->setWordWrap(true);
    auto sizePolicy = errorLabel->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    errorLabel->setSizePolicy(sizePolicy);
    errorLabel->setVisible(false);
}

void Utility::customizeHintLabel(QLabel *label)
{
    auto palette = label->palette();
    auto textColor = palette.color(QPalette::Text);
    textColor.setAlpha(128);
    palette.setColor(QPalette::Text, textColor);
    label->setPalette(palette);
}

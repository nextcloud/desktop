/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OWNCLOUDSETUPWIZARD_H
#define OWNCLOUDSETUPWIZARD_H

#include <QObject>
#include <QWidget>
#include <QPointer>
#include <QUrl>

class QQuickWindow;

namespace OCC {

class AccountWizardController;

/**
 * @brief The OwncloudSetupWizard class
 * @ingroup gui
 */
class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    /** Run the wizard */
    static void runWizard(QObject *obj, const char *amember, QWidget *parent = nullptr, bool forceRestart = false);
    static void runWizardForLoginFlow(QObject *obj, const char *amember, const QUrl &serverUrl, QWidget *parent = nullptr);
    static bool bringWizardToFrontIfVisible();

signals:
    // overall dialog close signal.
    void ownCloudWizardDone(int);

private:
    explicit OwncloudSetupWizard(QObject *parent = nullptr);
    ~OwncloudSetupWizard() override;
    bool startQmlWizard();
    bool startQmlWizardForLoginFlow(const QUrl &serverUrl);
    void startWizardForLoginFlow(const QUrl &serverUrl);
    void finish(int result);

    AccountWizardController *_qmlController = nullptr;
    QPointer<QQuickWindow> _qmlWizardWindow;
    bool _finished = false;
};
}

#endif // OWNCLOUDSETUPWIZARD_H

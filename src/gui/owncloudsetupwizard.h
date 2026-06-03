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
    static void runWizard(QObject *obj, const char *amember, QWidget *parent = nullptr);
    static bool bringWizardToFrontIfVisible();

signals:
    // overall dialog close signal.
    void ownCloudWizardDone(int);

private:
    explicit OwncloudSetupWizard(QObject *parent = nullptr);
    ~OwncloudSetupWizard() override;
    bool startQmlWizard();

    AccountWizardController *_qmlController = nullptr;
    QPointer<QQuickWindow> _qmlWizardWindow;
};
}

#endif // OWNCLOUDSETUPWIZARD_H

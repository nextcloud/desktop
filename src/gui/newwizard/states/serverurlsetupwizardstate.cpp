/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "serverurlsetupwizardstate.h"
#include "determineauthtypejobfactory.h"
#include "jobs/discoverwebfingerservicejobfactory.h"
#include "jobs/resolveurljobfactory.h"
#include "theme.h"

#include <QDebug>
#include <QMessageBox>

namespace {

const QString defaultUrlSchemeC = QStringLiteral("https://");
const QStringList supportedUrlSchemesC({ defaultUrlSchemeC, QStringLiteral("http://") });

}

namespace OCC::Wizard {

Q_LOGGING_CATEGORY(lcSetupWizardServerUrlState, "gui.setupwizard.states.serverurl");

ServerUrlSetupWizardState::ServerUrlSetupWizardState(SetupWizardContext *context)
    : AbstractSetupWizardState(context)
{
    auto serverUrl = [this]() {
        if (Theme::instance()->wizardEnableWebfinger()) {
            return _context->accountBuilder().legacyWebFingerServerUrl();
        } else {
            return _context->accountBuilder().serverUrl();
        }
    }();

    _page = new ServerUrlSetupWizardPage(serverUrl);
}

SetupWizardState ServerUrlSetupWizardState::state() const
{
    return SetupWizardState::ServerUrlState;
}

void ServerUrlSetupWizardState::evaluatePage()
{
    // we don't want to store any unnecessary certificates for this account when the user returns to the first page
    // the easiest way is to just reset the account builder
    _context->resetAccountBuilder();

    auto serverUrlSetupWizardPage = qobject_cast<ServerUrlSetupWizardPage *>(_page);
    Q_ASSERT(serverUrlSetupWizardPage != nullptr);

    const QUrl serverUrl = [serverUrlSetupWizardPage]() {
        QString userProvidedUrl = serverUrlSetupWizardPage->userProvidedUrl();

        // fix scheme if necessary
        // using HTTPS as a default is a real ly good idea nowadays, users can still enter http:// explicitly if they wish to
        if (!std::any_of(supportedUrlSchemesC.begin(), supportedUrlSchemesC.end(), [userProvidedUrl](const QString &scheme) {
                return userProvidedUrl.startsWith(scheme);
            })) {
            qInfo(lcSetupWizardServerUrlState) << "no URL scheme provided, prepending default URL scheme" << defaultUrlSchemeC;
            userProvidedUrl.prepend(defaultUrlSchemeC);
        }

        return QUrl::fromUserInput(userProvidedUrl).adjusted(QUrl::RemoveUserInfo);
    }();

    // (ab)use the account builder as temporary storage for the URL we are about to probe (after sanitation)
    // in case of errors, the user can just edit the previous value
    _context->accountBuilder().setServerUrl(serverUrl, DetermineAuthTypeJob::AuthType::Unknown);

    // TODO: perform some better validation
    if (!serverUrl.isValid()) {
        Q_EMIT evaluationFailed(tr("Invalid server URL"));
        return;
    }

    auto *messageBox = new QMessageBox(
        QMessageBox::Warning,
        tr("Insecure connection"),
        tr("The connection to %1 is insecure.\nAre you sure you want to proceed?").arg(serverUrl.toString()),
        QMessageBox::NoButton,
        _context->window());

    messageBox->setAttribute(Qt::WA_DeleteOnClose);

    messageBox->addButton(QMessageBox::Cancel);
    messageBox->addButton(tr("Confirm"), QMessageBox::YesRole);

    connect(messageBox, &QMessageBox::rejected, this, [this]() {
        Q_EMIT evaluationFailed(tr("Insecure server rejected by user"));
    });

    connect(messageBox, &QMessageBox::accepted, this, [this, serverUrl]() {
        // when moving back to this page (or retrying a failed credentials check), we need to make sure existing cookies
        // and certificates are deleted from the access manager
        _context->resetAccessManager();

        // since classic WebFinger is not enabled, we need to check whether modern (oCIS) WebFinger is available
        // therefore, we run the corresponding discovery job
        auto checkWebFingerAuthJob = Jobs::DiscoverWebFingerServiceJobFactory(_context->accessManager()).startJob(serverUrl, this);

        connect(checkWebFingerAuthJob, &CoreJob::finished, this, [job = checkWebFingerAuthJob, serverUrl, this]() {
            // in case any kind of error occurs, we assume the WebFinger service is not available
            if (!job->success()) {
                // first, we must resolve the actual server URL
                auto resolveJob = Jobs::ResolveUrlJobFactory(_context->accessManager()).startJob(serverUrl, this);

                connect(resolveJob, &CoreJob::finished, resolveJob, [this, resolveJob]() {
                    resolveJob->deleteLater();

                    if (!resolveJob->success()) {
                        Q_EMIT evaluationFailed(resolveJob->errorMessage());
                        return;
                    }

                    const auto resolvedUrl = resolveJob->result().toUrl();

                    // classic WebFinger workflow: auth type determination is delegated to whatever server the WebFinger service points us to in a dedicated
                    // step we can skip it here therefore
                    if (Theme::instance()->wizardEnableWebfinger()) {
                        _context->accountBuilder().setLegacyWebFingerServerUrl(resolvedUrl);
                        Q_EMIT evaluationSuccessful();
                        return;
                    }

                    // next, we need to find out which kind of authentication page we have to present to the user
                    auto authTypeJob = DetermineAuthTypeJobFactory(_context->accessManager()).startJob(resolvedUrl, this);

                    connect(authTypeJob, &CoreJob::finished, authTypeJob, [this, authTypeJob, resolvedUrl]() {
                        authTypeJob->deleteLater();

                        if (authTypeJob->result().isNull()) {
                            Q_EMIT evaluationFailed(authTypeJob->errorMessage());
                            return;
                        }

                        _context->accountBuilder().setServerUrl(resolvedUrl, qvariant_cast<DetermineAuthTypeJob::AuthType>(authTypeJob->result()));
                        Q_EMIT evaluationSuccessful();
                    });
                });

                connect(
                    resolveJob, &CoreJob::caCertificateAccepted, this,
                    [this](const QSslCertificate &caCertificate) {
                        // future requests made through this access manager should accept the certificate
                        _context->accessManager()->addCustomTrustedCaCertificates({caCertificate});

                        // the account maintains a list, too, which is also saved in the config file
                        _context->accountBuilder().addCustomTrustedCaCertificate(caCertificate);
                    },
                    Qt::DirectConnection);
            } else {
                _context->accountBuilder().setWebFingerAuthenticationServerUrl(job->result().toUrl());
                Q_EMIT evaluationSuccessful();
            }
        });
    });

    // instead of defining a lambda that we could call from here as well as the message box, we can put the
    // handler into the accepted() signal handler, and Q_EMIT that signal here
    if (serverUrl.scheme() == QStringLiteral("https")) {
        Q_EMIT messageBox->accepted();
    } else {
        messageBox->show();
    }
}

} // OCC::Wizard

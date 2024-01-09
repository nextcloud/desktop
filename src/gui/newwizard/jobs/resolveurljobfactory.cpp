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

#include "resolveurljobfactory.h"

#include "common/utility.h"
#include "gui/application.h"
#include "gui/owncloudgui.h"
#include "gui/settingsdialog.h"
#include "gui/tlserrordialog.h"
#include "gui/updateurldialog.h"

#include <QApplication>
#include <QNetworkReply>

namespace {
Q_LOGGING_CATEGORY(lcResolveUrl, "gui.wizard.resolveurl")

// used to signalize that the request was aborted intentionally by the sslErrorHandler
const char abortedBySslErrorHandlerC[] = "aborted-by-ssl-error-handler";
}

namespace OCC::Wizard::Jobs {

ResolveUrlJobFactory::ResolveUrlJobFactory(QNetworkAccessManager *nam)
    : AbstractCoreJobFactory(nam)
{
}

CoreJob *ResolveUrlJobFactory::startJob(const QUrl &url, QObject *parent)
{
    QNetworkRequest req(Utility::concatUrlPath(url, QStringLiteral("status.php")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *job = new CoreJob(nam()->get(req), parent);

    auto makeFinishedHandler = [=](QNetworkReply *reply) {
        return [oldUrl = url, reply, job] {
            if (reply->error() != QNetworkReply::NoError) {
                if (reply->property(abortedBySslErrorHandlerC).toBool()) {
                    return;
                }

                qCCritical(lcResolveUrl) << QStringLiteral("Failed to resolve URL %1, error: %2").arg(oldUrl.toDisplayString(), reply->errorString());

                setJobError(job, QApplication::translate("ResolveUrlJobFactory", "Could not detect compatible server at %1").arg(oldUrl.toDisplayString()));
                qCWarning(lcResolveUrl) << job->errorMessage();
                return;
            }

            const auto newUrl = reply->url().adjusted(QUrl::RemoveFilename);

            if (newUrl != oldUrl) {
                qCInfo(lcResolveUrl) << oldUrl << "was redirected to" << newUrl;

                if (newUrl.scheme() == QLatin1String("https") && oldUrl.host() == newUrl.host()) {
                    qCInfo(lcResolveUrl()) << "redirect accepted automatically";
                    setJobResult(job, newUrl);
                } else {
                    auto *dialog = new UpdateUrlDialog(
                        QStringLiteral("Confirm new URL"),
                        QStringLiteral(
                            "While accessing the server, we were redirected from %1 to another URL: %2\n\n"
                            "Do you wish to permanently use the new URL?")
                            .arg(oldUrl.toString(), newUrl.toString()),
                        oldUrl,
                        newUrl,
                        nullptr);

                    QObject::connect(dialog, &UpdateUrlDialog::accepted, job, [=]() {
                        setJobResult(job, newUrl);
                    });

                    QObject::connect(dialog, &UpdateUrlDialog::rejected, job, [=]() {
                        setJobError(job, QApplication::translate("ResolveUrlJobFactory", "User rejected redirect from %1 to %2").arg(oldUrl.toDisplayString(), newUrl.toDisplayString()));
                    });

                    dialog->show();
                }
            } else {
                setJobResult(job, newUrl);
            }
        };
    };

    QObject::connect(job->reply(), &QNetworkReply::finished, job, makeFinishedHandler(job->reply()));

    QObject::connect(job->reply(), &QNetworkReply::sslErrors, job, [req, job, makeFinishedHandler, nam = nam()](const QList<QSslError> &errors) mutable {
        auto *tlsErrorDialog = new TlsErrorDialog(errors, job->reply()->url().host(), ocApp()->gui()->settingsDialog());

        job->reply()->setProperty(abortedBySslErrorHandlerC, true);
        job->reply()->abort();

        QObject::connect(tlsErrorDialog, &TlsErrorDialog::accepted, job, [job, req, errors, nam, makeFinishedHandler]() mutable {
            for (const auto &error : errors) {
                Q_EMIT job->caCertificateAccepted(error.certificate());
            }
            auto *reply = nam->get(req);
            QObject::connect(reply, &QNetworkReply::finished, job, makeFinishedHandler(reply));
        });

        QObject::connect(tlsErrorDialog, &TlsErrorDialog::rejected, job, [job]() {
            setJobError(job, QApplication::translate("ResolveUrlJobFactory", "User rejected invalid SSL certificate"));
        });

        ownCloudGui::raise();
        tlsErrorDialog->open();
    });

    makeRequest();

    return job;
}
}

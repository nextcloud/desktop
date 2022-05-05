#include "resolveurljobfactory.h"

#include "accessmanager.h"
#include "common/utility.h"
#include "gui/tlserrordialog.h"
#include "gui/updateurldialog.h"

#include <QNetworkReply>

namespace {
Q_LOGGING_CATEGORY(lcResolveUrl, "wizard.resolveurl")

// used to signalize that the request was aborted intentionally by the sslErrorHandler
const char abortedBySslErrorHandlerC[] = "aborted-by-ssl-error-handler";
}

namespace OCC::Wizard::Jobs {

ResolveUrlJobFactory::ResolveUrlJobFactory(QNetworkAccessManager *nam, QObject *parent)
    : AbstractCoreJobFactory(nam, parent)
{
}

CoreJob *ResolveUrlJobFactory::startJob(const QUrl &url)
{
    auto *job = new CoreJob;

    QNetworkRequest req(Utility::concatUrlPath(url, QStringLiteral("status.php")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);

    auto *reply = nam()->get(req);

    auto makeFinishedHandler = [=](QNetworkReply *reply) {
        return [oldUrl = url, reply, job] {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (reply->property(abortedBySslErrorHandlerC).toBool()) {
                    return;
                }

                setJobError(job, tr("Failed to resolve the url %1, error: %2").arg(oldUrl.toDisplayString(), reply->errorString()), reply->error());
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

                    connect(dialog, &UpdateUrlDialog::accepted, job, [=]() {
                        setJobResult(job, newUrl);
                    });

                    connect(dialog, &UpdateUrlDialog::accepted, job, [=]() {
                        setJobResult(job, newUrl);
                    });

                    connect(dialog, &UpdateUrlDialog::rejected, job, [=]() {
                        setJobError(job, tr("User rejected redirect from %1 to %2").arg(oldUrl.toDisplayString(), newUrl.toDisplayString()), QNetworkReply::InsecureRedirectError);
                    });

                    dialog->show();
                }
            } else {
                setJobResult(job, newUrl);
            }
        };
    };

    connect(reply, &QNetworkReply::finished, job, makeFinishedHandler(reply));

    connect(reply, &QNetworkReply::sslErrors, reply, [reply, req, job, makeFinishedHandler, nam = nam()](const QList<QSslError> &errors) mutable {
        auto *tlsErrorDialog = new TlsErrorDialog(errors, reply->url().host());

        reply->setProperty(abortedBySslErrorHandlerC, true);
        reply->abort();

        connect(tlsErrorDialog, &TlsErrorDialog::accepted, job, [job, req, errors, nam, makeFinishedHandler]() mutable {
            for (const auto &error : errors) {
                Q_EMIT job->caCertificateAccepted(error.certificate());
            }
            auto *reply = nam->get(req);
            connect(reply, &QNetworkReply::finished, job, makeFinishedHandler(reply));
        });

        connect(tlsErrorDialog, &TlsErrorDialog::rejected, job, [job]() {
            setJobError(job, tr("User rejected invalid SSL certificate"), QNetworkReply::SslHandshakeFailedError);
        });

        tlsErrorDialog->show();
    });

    makeRequest();

    return job;
}

}

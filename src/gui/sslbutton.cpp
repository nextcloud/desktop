/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sslbutton.h"
#include "account.h"
#include "accountstate.h"
#include "theme.h"

#include <QMenu>
#include <QUrl>
#include <QtNetwork>
#include <QSslConfiguration>
#include <QWidgetAction>
#include <QLabel>

namespace OCC {

Q_LOGGING_CATEGORY(lcSsl, "nextcloud.gui.ssl", QtInfoMsg)

SslButton::SslButton(QWidget *parent)
    : QToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
    setAutoRaise(true);

    _menu = new QMenu(this);
    QObject::connect(_menu, &QMenu::aboutToShow,
        this, &SslButton::slotUpdateMenu);
    setMenu(_menu);
}

static QString addCertDetailsField(const QString &key, const QString &value)
{
    if (value.isEmpty())
        return QString();

    return QLatin1String("<tr><td style=\"vertical-align: top;\"><b>") + key
        + QLatin1String("</b></td><td style=\"vertical-align: bottom;\">") + value
        + QLatin1String("</td></tr>");
}


// necessary indication only, not sufficient for primary validation!
static bool isSelfSigned(const QSslCertificate &certificate)
{
    return certificate.issuerInfo(QSslCertificate::CommonName) == certificate.subjectInfo(QSslCertificate::CommonName)
        && certificate.issuerInfo(QSslCertificate::OrganizationalUnitName) == certificate.subjectInfo(QSslCertificate::OrganizationalUnitName);
}

QMenu *SslButton::buildCertMenu(QMenu *parent, const QSslCertificate &cert,
    const QList<QSslCertificate> &userApproved, int pos, const QList<QSslCertificate> &systemCaCertificates)
{
    QString cn = QStringList(cert.subjectInfo(QSslCertificate::CommonName)).join(QChar(';'));
    QString ou = QStringList(cert.subjectInfo(QSslCertificate::OrganizationalUnitName)).join(QChar(';'));
    QString org = QStringList(cert.subjectInfo(QSslCertificate::Organization)).join(QChar(';'));
    QString country = QStringList(cert.subjectInfo(QSslCertificate::CountryName)).join(QChar(';'));
    QString state = QStringList(cert.subjectInfo(QSslCertificate::StateOrProvinceName)).join(QChar(';'));
    QString issuer = QStringList(cert.issuerInfo(QSslCertificate::CommonName)).join(QChar(';'));
    if (issuer.isEmpty())
        issuer = QStringList(cert.issuerInfo(QSslCertificate::OrganizationalUnitName)).join(QChar(';'));
    QString sha1 = Utility::formatFingerprint(cert.digest(QCryptographicHash::Sha1).toHex(), false);
    QByteArray sha265hash = cert.digest(QCryptographicHash::Sha256).toHex();
    QString sha256escaped =
        Utility::escape(Utility::formatFingerprint(sha265hash.left(sha265hash.length() / 2), false))
        + QLatin1String("<br/>")
        + Utility::escape(Utility::formatFingerprint(sha265hash.mid(sha265hash.length() / 2), false));
    QString serial = QString::fromUtf8(cert.serialNumber());
    QString effectiveDate = cert.effectiveDate().date().toString();
    QString expiryDate = cert.expiryDate().date().toString();
    QString sna = QStringList(cert.subjectAlternativeNames().values()).join(" ");

    QString details;
    QTextStream stream(&details);

    stream << QLatin1String("<html><body>");

    stream << tr("<h3>Certificate Details</h3>");

    stream << QLatin1String("<table>");
    stream << addCertDetailsField(tr("Common Name (CN):"), Utility::escape(cn));
    stream << addCertDetailsField(tr("Subject Alternative Names:"), Utility::escape(sna).replace(" ", "<br/>"));
    stream << addCertDetailsField(tr("Organization (O):"), Utility::escape(org));
    stream << addCertDetailsField(tr("Organizational Unit (OU):"), Utility::escape(ou));
    stream << addCertDetailsField(tr("State/Province:"), Utility::escape(state));
    stream << addCertDetailsField(tr("Country:"), Utility::escape(country));
    stream << addCertDetailsField(tr("Serial:"), Utility::escape(serial));
    stream << QLatin1String("</table>");

    stream << tr("<h3>Issuer</h3>");

    stream << QLatin1String("<table>");
    stream << addCertDetailsField(tr("Issuer:"), Utility::escape(issuer));
    stream << addCertDetailsField(tr("Issued on:"), Utility::escape(effectiveDate));
    stream << addCertDetailsField(tr("Expires on:"), Utility::escape(expiryDate));
    stream << QLatin1String("</table>");

    stream << tr("<h3>Fingerprints</h3>");

    stream << QLatin1String("<table>");

    stream << addCertDetailsField(tr("SHA-256:"), sha256escaped);
    stream << addCertDetailsField(tr("SHA-1:"), Utility::escape(sha1));
    stream << QLatin1String("</table>");

    if (userApproved.contains(cert)) {
        stream << tr("<p><b>Note:</b> This certificate was manually approved</p>");
    }
    stream << QLatin1String("</body></html>");

    QString txt;
    if (pos > 0) {
        txt += QString(2 * pos, ' ');
        if (!Utility::isWindows()) {
            // doesn't seem to work reliably on Windows
            txt += QChar(0x21AA); // nicer '->' symbol
            txt += QChar(' ');
        }
    }

    QString certId = cn.isEmpty() ? ou : cn;

    if (systemCaCertificates.contains(cert)) {
        txt += certId;
    } else {
        if (isSelfSigned(cert)) {
            txt += tr("%1 (self-signed)").arg(certId);
        } else {
            txt += tr("%1").arg(certId);
        }
    }

    // create label first
    auto *label = new QLabel(parent);
    label->setStyleSheet(QLatin1String("QLabel { padding: 8px; }"));
    label->setTextFormat(Qt::RichText);
    label->setText(details);

    // plug label into widget action
    auto *action = new QWidgetAction(parent);
    action->setDefaultWidget(label);
    // plug action into menu
    auto *menu = new QMenu(parent);
    menu->menuAction()->setText(txt);
    menu->addAction(action);

    return menu;
}

void SslButton::updateAccountState(AccountState *accountState)
{
    if (!accountState || !accountState->isConnected()) {
        setVisible(false);
        return;
    } else {
        setVisible(true);
    }
    _accountState = accountState;

    AccountPtr account = _accountState->account();
    if (account->url().scheme() == QLatin1String("https")) {
        setIcon(QIcon(QLatin1String(":/client/theme/lock-https.svg")));
        QSslCipher cipher = account->_sessionCipher;
        setToolTip(tr("This connection is encrypted using %1 bit %2.\n").arg(cipher.usedBits()).arg(cipher.name()));
    } else {
        setIcon(QIcon(QLatin1String(":/client/theme/lock-broken.svg")));
        setToolTip(tr("This connection is NOT secure as it is not encrypted.\n"));
    }
}

void SslButton::slotUpdateMenu()
{
    _menu->clear();

    if (!_accountState) {
        return;
    }

    AccountPtr account = _accountState->account();

    _menu->addAction(tr("Server version: %1").arg(account->serverVersion()))->setEnabled(false);

    if (account->isHttp2Supported()) {
        _menu->addAction("HTTP/2")->setEnabled(false);
    }

    if (account->url().scheme() == QLatin1String("https")) {
        QString sslVersion = account->_sessionCipher.protocolString()
            + ", " + account->_sessionCipher.authenticationMethod()
            + ", " + account->_sessionCipher.keyExchangeMethod()
            + ", " + account->_sessionCipher.encryptionMethod();
        _menu->addAction(sslVersion)->setEnabled(false);

        if (account->_sessionTicket.isEmpty()) {
            _menu->addAction(tr("No support for SSL session tickets/identifiers"))->setEnabled(false);
        }

        QList<QSslCertificate> chain = account->_peerCertificateChain;

        if (chain.isEmpty()) {
            qCWarning(lcSsl) << "Empty certificate chain";
            return;
        }

        _menu->addAction(tr("Certificate information:"))->setEnabled(false);

        const auto systemCerts = QSslConfiguration::systemCaCertificates();

        QList<QSslCertificate> tmpChain;
        for (const auto &cert : chain) {
            tmpChain << cert;
            if (systemCerts.contains(cert))
                break;
        }
        chain = tmpChain;

        // find trust anchor (informational only, verification is done by QSslSocket!)
        for (const auto &rootCA : systemCerts) {
            if (rootCA.issuerInfo(QSslCertificate::CommonName) == chain.last().issuerInfo(QSslCertificate::CommonName)
                && rootCA.issuerInfo(QSslCertificate::Organization) == chain.last().issuerInfo(QSslCertificate::Organization)) {
                chain.append(rootCA);
                break;
            }
        }

        QListIterator<QSslCertificate> it(chain);
        it.toBack();
        int i = 0;
        while (it.hasPrevious()) {
            _menu->addMenu(buildCertMenu(_menu, it.previous(), account->approvedCerts(), i, systemCerts));
            i++;
        }
    } else {
        _menu->addAction(tr("The connection is not secure"))->setEnabled(false);
    }
}

} // namespace OCC

/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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
    QLabel *label = new QLabel(parent);
    label->setStyleSheet(QLatin1String("QLabel { padding: 8px; background-color: #fff; }"));
    label->setTextFormat(Qt::RichText);
    label->setText(details);

    // plug label into widget action
    QWidgetAction *action = new QWidgetAction(parent);
    action->setDefaultWidget(label);
    // plug action into menu
    QMenu *menu = new QMenu(parent);
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
        setIcon(QIcon(QLatin1String(":/client/resources/lock-https.png")));
        QSslCipher cipher = account->_sessionCipher;
        setToolTip(tr("This connection is encrypted using %1 bit %2.\n").arg(cipher.usedBits()).arg(cipher.name()));
        setMenu(_menu);
    } else {
        setIcon(QIcon(QLatin1String(":/client/resources/lock-http.png")));
        setToolTip(tr("This connection is NOT secure as it is not encrypted.\n"));
        setMenu(0);
    }
}

void SslButton::slotUpdateMenu()
{
    _menu->clear();

    if (!_accountState) {
        return;
    }

    AccountPtr account = _accountState->account();

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
        foreach (QSslCertificate cert, chain) {
            tmpChain << cert;
            if (systemCerts.contains(cert))
                break;
        }
        chain = tmpChain;

        // find trust anchor (informational only, verification is done by QSslSocket!)
        for (const QSslCertificate &rootCA : systemCerts) {
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
    }
}

} // namespace OCC

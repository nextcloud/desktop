/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "mirall/sslbutton.h"
#include "mirall/account.h"
#include "mirall/utility.h"

#include <QMenu>
#include <QUrl>
#include <QtNetwork>
#include <QSslConfiguration>
#include <openssl/x509.h>

namespace Mirall {

SslButton::SslButton(QWidget *parent) :
    QToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
}

QString SslButton::protoToString(QSsl::SslProtocol proto)
{
    switch(proto) {
        break;
    case QSsl::SslV2:
        return QLatin1String("SSL v2");
    case QSsl::SslV3:
        return QLatin1String("SSL v3");
    case QSsl::TlsV1:
        return QLatin1String("TLS");
    default:
        return QString();
    }
}

static QString escapeAmps(QString str)
{
    return str.replace('&', "&&");
}

QMenu* SslButton::buildCertMenu(QMenu *parent, const QSslCertificate& cert,
                                const QList<QSslCertificate>& userApproved, int pos)
{
    QString cn = cert.subjectInfo(QSslCertificate::CommonName);
    QString ou = cert.subjectInfo(QSslCertificate::OrganizationalUnitName);
    QString org = cert.subjectInfo(QSslCertificate::Organization);
    QString country = cert.subjectInfo(QSslCertificate::CountryName);
    QString state = cert.subjectInfo(QSslCertificate::StateOrProvinceName);
    QString issuer = cert.issuerInfo(QSslCertificate::CommonName);
    QString md5 = Utility::formatFingerprint(cert.digest(QCryptographicHash::Md5).toHex());
    QString sha1 = Utility::formatFingerprint(cert.digest(QCryptographicHash::Sha1).toHex());
    QString serial = QString::fromUtf8(cert.serialNumber());
    QString effectiveDate = cert.effectiveDate().date().toString();
    QString expiryDate = cert.expiryDate().date().toString();
    QString sna = QStringList(cert.alternateSubjectNames().values()).join(", ");

    QMenu *details = new QMenu(parent);
    details->addAction(tr("Common Name (CN): %1").arg(escapeAmps(cn)))->setEnabled(false);
    if (!sna.isEmpty()) {
        details->addAction(tr("Subject Alternative Names: %1").arg(escapeAmps(sna)))->setEnabled(false);
    }
    if (!org.isEmpty()) {
    details->addAction(tr("Organization (O): %1").arg(escapeAmps(org)))->setEnabled(false);
    }
    if (!ou.isEmpty()) {
    details->addAction(tr("Organizational Unit (OU): %1").arg(escapeAmps(ou)))->setEnabled(false);
    }
    if (!country.isEmpty()) {
    details->addAction(tr("Country: %1").arg(escapeAmps(country)))->setEnabled(false);
    }
    if (!state.isEmpty()) {
    details->addAction(tr("State/Province: %1").arg(escapeAmps(state)))->setEnabled(false);
    }
    details->addAction(tr("Serial: %1").arg(escapeAmps(serial)))->setEnabled(false);
    details->addAction(tr("Issuer: %1").arg(escapeAmps(issuer)))->setEnabled(false);
    details->addAction(tr("Issued on: %1").arg(effectiveDate))->setEnabled(false);
    details->addAction(tr("Expires on: %1").arg(expiryDate))->setEnabled(false);
    details->addSeparator();
    details->addAction(tr("Fingerprints:"))->setEnabled(false);
    details->addAction(tr("MD 5: %1").arg(md5))->setEnabled(false);
    details->addAction(tr("SHA-1: %1").arg(sha1))->setEnabled(false);
    if (userApproved.contains(cert)) {
        details->addSeparator();
        details->addAction(tr("This certificate was manually approved"))->setEnabled(false);
    }


    QString txt;
    if (pos > 0) {
        txt += QString(pos, ' ');
        txt += QChar(0x21AA); // nicer '->' symbol
        txt += QChar(' ');
    }

    if (QSslSocket::systemCaCertificates().contains(cert)) {
        txt += tr("%1 (in Root CA store)").arg(cn);
    } else {
        if (cn == issuer) {
            txt += tr("%1 (self-signed)").arg(cn, issuer);
        } else {
            txt += tr("%1").arg(cn);
        }
    }
    details->menuAction()->setText(txt);
    return details;

}

void SslButton::updateAccountInfo(Account *account)
{
    if (!account || account->state() != Account::Connected) {
        setVisible(false);
        return;
    } else {
        setVisible(true);
    }
    if (account->url().scheme() == QLatin1String("https")) {
        setIcon(QIcon(QPixmap(":/mirall/resources/lock-https.png")));
        QSslCipher cipher = account->sslConfiguration().sessionCipher();
        setToolTip(tr("This connection is encrypted using %1 bit %2.\n").arg(cipher.usedBits()).arg(cipher.name()));
        QMenu *menu = new QMenu(this);
        QList<QSslCertificate> chain = account->sslConfiguration().peerCertificateChain();
        menu->addAction(tr("Certificate information:"))->setEnabled(false);

        // find trust anchor (informational only, verification is done by QSslSocket!)
        foreach (const QSslCertificate &rootCA, QSslSocket::systemCaCertificates()) {
            QString cn = chain.last().issuerInfo(QSslCertificate::CommonName);\
            QString org = chain.last().issuerInfo(QSslCertificate::Organization);\
            if (rootCA.issuerInfo(QSslCertificate::CommonName) == cn &&
                rootCA.issuerInfo(QSslCertificate::Organization) == org) {
                chain << rootCA;
                break;
            }
        }

        QListIterator<QSslCertificate> it(chain);
        it.toBack();
        int i = 0;
        while (it.hasPrevious()) {
            menu->addMenu(buildCertMenu(menu, it.previous(), account->approvedCerts(), i));
            i++;
        }
        setMenu(menu);
    } else {
        setIcon(QIcon(QPixmap(":/mirall/resources/lock-http.png")));
        setToolTip(tr("This connection is NOT secure as it is not encrypted.\n"));
        setMenu(0);
    }
}

} // namespace Mirall

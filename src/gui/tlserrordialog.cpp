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

#include "tlserrordialog.h"
#include "common/utility.h"
#include "ui_tlserrordialog.h"

#include <QAbstractButton>
#include <QPushButton>

namespace OCC {
TlsErrorDialog::TlsErrorDialog(const QList<QSslError> &sslErrors, const QString &host, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::TlsErrorDialog)
{
    _ui->setupUi(this);

    _ui->hostnameLabel->setText(tr("Cannot connect securely to %1").arg(host));

    QStringList errorStrings;

    for (const auto &error : sslErrors) {
        errorStrings << error.errorString() << describeCertificateHtml(error.certificate());
    }

    _ui->textBrowser->setHtml(errorStrings.join(QStringLiteral("\n")));

    // FIXME: add checkbox for second confirmation

    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        accept();
    });
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        reject();
    });

    // of course, we require an answer from the user, they may not proceed with anything else
    setModal(true);
}

TlsErrorDialog::~TlsErrorDialog()
{
    delete _ui;
}

QString TlsErrorDialog::describeCertificateHtml(const QSslCertificate &certificate)
{
    auto formatFingerprint = [certificate](QCryptographicHash::Algorithm algorithm) {
        return Utility::escape(QString::fromUtf8(certificate.digest(algorithm).toHex()));
    };

    auto formatInfo = [](const QStringList &stringList) {
        return Utility::escape(stringList.join(QStringLiteral(", ")));
    };

    auto escapeValueOrNotSpecified = [&](const QStringList &stringList) {
        if (stringList.isEmpty()) {
            return tr("&lt;not specified&gt;");
        } else {
            return formatInfo(stringList);
        }
    };

    QString msg = tr(
        "<div id=\"cert\">"
        "<h3>with Certificate %1</h3>"
        "<div id=\"ccert\">"
        "<p>"
        "Organization: %2<br/>"
        "Unit: %3<br/>"
        "Country: %4"
        "</p>"
        "<p>"
        "Fingerprint (MD5): <tt>%5</tt><br/>"
        "Fingerprint (SHA1): <tt>%6</tt><br/>"
        "Fingerprint (SHA256): <tt>%7</tt><br/>"
        "<br/>"
        "Effective Date: %8"
        "Expiration Date: %9"
        "</div>"
        "<h3>Issuer: %10</h3>"
        "<div id=\"issuer\">"
        "<p>"
        "Organization: %11<br/>"
        "Unit: %12<br/>"
        "Country: %13"
        "</p>"
        "</div>"
        "</div>")
                      .arg(
                          formatInfo(certificate.subjectInfo(QSslCertificate::CommonName)),
                          escapeValueOrNotSpecified(certificate.subjectInfo(QSslCertificate::Organization)),
                          escapeValueOrNotSpecified(certificate.subjectInfo(QSslCertificate::OrganizationalUnitName)),
                          escapeValueOrNotSpecified(certificate.subjectInfo(QSslCertificate::CountryName)),
                          formatFingerprint(QCryptographicHash::Md5),
                          formatFingerprint(QCryptographicHash::Sha1),
                          formatFingerprint(QCryptographicHash::Sha256),
                          certificate.effectiveDate().toString(),
                          certificate.expiryDate().toString(),
                          formatInfo(certificate.issuerInfo(QSslCertificate::CommonName)),
                          escapeValueOrNotSpecified(certificate.issuerInfo(QSslCertificate::Organization)),
                          escapeValueOrNotSpecified(certificate.issuerInfo(QSslCertificate::OrganizationalUnitName)),
                          escapeValueOrNotSpecified(certificate.issuerInfo(QSslCertificate::CountryName)));

    return msg;
}

} // OCC

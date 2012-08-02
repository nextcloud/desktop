/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
#include "sslerrordialog.h"
#include "mirall/mirallconfigfile.h"

#include <QtGui>
#include <QtNetwork>

namespace Mirall
{
#define CA_CERTS_KEY QLatin1String("CaCertificates")

SslErrorDialog::SslErrorDialog(QWidget *parent) :
    QDialog(parent), _allTrusted(false)
{
    setupUi( this  );
    setWindowTitle( tr("SSL Connection") );
    QPushButton *okButton = _dialogButtonBox->button( QDialogButtonBox::Ok );

    if( okButton ) {
        okButton->setDefault(true);
        connect( okButton, SIGNAL(clicked()),SLOT(accept()));
    }
}

QList<QSslCertificate> SslErrorDialog::storedCACerts()
{
    MirallConfigFile cfg( _customConfigHandle );
    QSettings settings( cfg.configFile(), QSettings::IniFormat);

    QList<QSslCertificate> cacerts = QSslCertificate::fromData(settings.value(CA_CERTS_KEY).toByteArray());

    return cacerts;
}

QString SslErrorDialog::styleSheet() const
{
    const QString style = QLatin1String(
                "#cert {margin-left: 5px;} "
                "#ca_error { color:#a00011; margin-left:5px; margin-right:5px; }"
                "#ca_error p { margin-top: 2px; margin-bottom:2px; }"
                "#ccert { margin-left: 5px; }"
                "#issuer { margin-left: 5px; }"
                );

    return style;
}
#define QL(x) QLatin1String(x)

bool SslErrorDialog::setErrorList( QList<QSslError> errors )
{
    QList<QSslCertificate> ourCerts = storedCACerts();

    // check if unknown certs caused errors.
    _unknownCerts.clear();

    QStringList errorStrings;
    for (int i = 0; i < errors.count(); ++i) {
        if (ourCerts.contains(errors.at(i).certificate()) ||
                _unknownCerts.contains(errors.at(i).certificate() ))
            continue;
        errorStrings += errors.at(i).errorString();
        if (!errors.at(i).certificate().isNull()) {
            _unknownCerts.append(errors.at(i).certificate());
        }
    }

    // if there are no errors left, all Certs were known.
    if (errorStrings.isEmpty()) {
        _allTrusted = true;
        return true;
    }

    QString msg = QL("<html><head>");
            msg += QL("<link rel='stylesheet' type='text/css' href='format.css'>");
            msg += QL("</head><body>");

    // loop over the unknown certs and line up their errors.
            msg += QL("<h3>") + tr("Warnings about current SSL Connection:") + QL("</h3>");
    msg += QL("<div id=\"ca_errors\">");
    foreach( const QSslCertificate& cert, _unknownCerts ) {
        msg += QL("<div id=\"ca_error\">");
        // add the errors for this cert
        foreach( QSslError err, errors ) {
            if( err.certificate() == cert ) {
                msg += "<p>" + err.errorString() + "</p>";
            }
        }
        msg += QL("</div>");
        msg += certDiv( cert );
        if( _unknownCerts.count() > 1 ) {
            msg += QL("<hr/>");
        }
    }
    msg += QL("</div></body></html>");

    qDebug() << "#  # # # # # ";
    qDebug() << msg;
    QTextDocument *doc = new QTextDocument(0);
    QString style = styleSheet();
    qDebug() << "Style: " << style;
    doc->addResource( QTextDocument::StyleSheetResource, QUrl( "format.css" ), style);
    doc->setHtml( msg );

    _tbErrors->setDocument( doc );
    _tbErrors->show();

    return false;
}

QString SslErrorDialog::certDiv( QSslCertificate cert ) const
{
    QString msg;
    msg += QL("<div id=\"cert\">");
    msg += QL("<h3>") + tr("with Certificate %1").arg( cert.subjectInfo( QSslCertificate::CommonName )) + QL("</h3>");

    msg += QL("<div id=\"ccert\">");
    QStringList li;
    li << tr("Organization: %1").arg( cert.subjectInfo( QSslCertificate::Organization) );
    li << tr("Unit: %1").arg( cert.subjectInfo( QSslCertificate::OrganizationalUnitName) );
    li << tr("Country: %1").arg(cert.subjectInfo( QSslCertificate::CountryName));
    msg += QL("<p>") + li.join("<br/>") + QL("</p>");

    msg += QL("<p>");
    msg += tr("Effective Date: %1").arg( cert.effectiveDate().toString()) + QL("<br/>");
    msg += tr("Expiry Date: %1").arg( cert.expiryDate().toString()) + QL("</p>");

    msg += QL("</div>" );

    msg += QL("<h3>") + tr("Issuer: %1").arg( cert.issuerInfo( QSslCertificate::CommonName )) + QL("</h3>");
    msg += "<div id=\"issuer\">";
    li.clear();
    li << tr("Organization: %1").arg( cert.issuerInfo( QSslCertificate::Organization) );
    li << tr("Unit: %1").arg( cert.issuerInfo( QSslCertificate::OrganizationalUnitName) );
    li << tr("Country: %1").arg(cert.issuerInfo( QSslCertificate::CountryName));
    msg += QL("<p>") + li.join("<br/>") + QL("</p>");
    msg += QL("</div>" );
    msg += QL("</div>" );

    return msg;
}

bool SslErrorDialog::trustConnection()
{
    if( _allTrusted ) return true;

    bool stat = ( _cbTrustConnect->checkState() == Qt::Checked );
    qDebug() << "SSL-Connection is trusted: " << stat;

    return stat;
}

void SslErrorDialog::accept()
{
    // Save the contents of _unknownCerts to the settings file.
    if( trustConnection() && _unknownCerts.count() > 0 ) {
        QSslSocket::addDefaultCaCertificates(_unknownCerts);

        MirallConfigFile cfg( _customConfigHandle );
        QSettings settings( cfg.configFile(), QSettings::IniFormat);

        QByteArray certs = settings.value(CA_CERTS_KEY).toByteArray();
        qDebug() << "Saving " << _unknownCerts.count() << " unknown certs.";
        foreach( const QSslCertificate& cert, _unknownCerts ) {
            certs += cert.toPem() + '\n';
        }
        settings.setValue(CA_CERTS_KEY, certs);
    }

    QDialog::accept();
}

void SslErrorDialog::setCustomConfigHandle( const QString& handle )
{
    _customConfigHandle = handle;
}

} // end namespace

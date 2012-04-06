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

#include <QtNetwork>

#include "sslerrordialog.h"

namespace Mirall
{

SslErrorDialog::SslErrorDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi( this  );
    setWindowTitle( tr("SSL Connection") );



}

void SslErrorDialog::setErrorList( QList<QSslError> errors )
{
    QString msg;

    QMap<QByteArray, QStringList> certErrors;
    QMap<QByteArray, QString> certInfo;

    foreach( QSslError err, errors ) {
        QSslCertificate cert = err.certificate();
        QByteArray digest = cert.digest();
        QStringList errorList;

        if( certInfo.contains( digest )) {
            // the cert is known already.
            if( certErrors.contains(digest) ) {
                errorList = certErrors.value(digest);
            } else {
                qDebug() << "WRN: No certErrors but info exists!";
            }
        } else {
            certInfo[digest] = certDiv( cert );
        }
        errorList.append( err.errorString() );
        certErrors[digest] = errorList;
    }

    // Loop over map and show the errors per certificate.

    _tbErrors->setText( msg );
}

QString SslErrorDialog::certDiv( QSslCertificate cert ) const
{
    QString msg;
    msg += "<div id=\"cert\">";
    msg += QString( "<h2>Certificate MD5 %1</h2>" ).arg( QString::fromAscii(cert.digest() ));
    msg += "<div id=\"ccert\">";
    msg += QString( "<p>Name: %1</p>").arg( cert.subjectInfo( QSslCertificate::CommonName ) );
    msg += QString( "<p>Organization: %1</p>").arg( cert.subjectInfo( QSslCertificate::Organization) );
    msg += QString( "<p>Unit: %1</p>").arg( cert.subjectInfo( QSslCertificate::OrganizationalUnitName) );
    msg += QString( "<p>Country: %1</p>").arg( cert.subjectInfo( QSslCertificate::CountryName) );
    msg += "</div>";
    msg += "<div id=\"issuer\">";
    msg += QString( "<p>Name: %1</p>").arg( cert.issuerInfo( QSslCertificate::CommonName ) );
    msg += QString( "<p>Organization: %1</p>").arg( cert.issuerInfo( QSslCertificate::Organization) );
    msg += QString( "<p>Unit: %1</p>").arg( cert.issuerInfo( QSslCertificate::OrganizationalUnitName) );
    msg += QString( "<p>Country: %1</p>").arg( cert.issuerInfo( QSslCertificate::CountryName) );
    msg += "</div>";
    msg += "</div>";
    msg += "</div>";

    return msg;
}




} // end namespace

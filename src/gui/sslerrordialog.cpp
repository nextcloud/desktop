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
#include "configfile.h"
#include "utility.h"
#include "sslerrordialog.h"

#include <QtGui>
#include <QtNetwork>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif


#include "ui_sslerrordialog.h"

namespace OCC
{

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
namespace Utility {
    //  Used for QSSLCertificate::subjectInfo which returns a QStringList in Qt5, but a QString in Qt4
    QString escape(const QStringList &l) { return escape(l.join(';')); }
}
#endif

bool SslDialogErrorHandler::handleErrors(QList<QSslError> errors, QList<QSslCertificate> *certs, Account *account)
{
    if (!certs) {
        qDebug() << "Certs parameter required but is NULL!";
        return false;
    }

    SslErrorDialog dlg(account);
    // whether the failing certs have previously been accepted
    if (dlg.checkFailingCertsKnown(errors)) {
        *certs = dlg.unknownCerts();
        return true;
    }
    // whether the user accepted the certs
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.trustConnection()) {
            *certs = dlg.unknownCerts();
            return true;
        }
    }
    return false;
}

SslErrorDialog::SslErrorDialog(Account *account, QWidget *parent) :
    QDialog(parent), _allTrusted(false), _ui(new Ui::SslErrorDialog), _account(account)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    _ui->setupUi( this );
    setWindowTitle( tr("SSL Connection") );
    QPushButton *okButton =
            _ui->_dialogButtonBox->button( QDialogButtonBox::Ok );
    QPushButton *cancelButton =
           _ui->_dialogButtonBox->button( QDialogButtonBox::Cancel );
    okButton->setEnabled(false);
    connect(_ui->_cbTrustConnect, SIGNAL(clicked(bool)),
            okButton, SLOT(setEnabled(bool)));

    if( okButton ) {
        okButton->setDefault(true);
        connect( okButton, SIGNAL(clicked()),SLOT(accept()));
        connect( cancelButton, SIGNAL(clicked()),SLOT(reject()));
    }
}

SslErrorDialog::~SslErrorDialog()
{
    delete _ui;
}


QString SslErrorDialog::styleSheet() const
{
    const QString style = QLatin1String(
                "#cert {margin-left: 5px;} "
                "#ca_error { color:#a00011; margin-left:5px; margin-right:5px; }"
                "#ca_error p { margin-top: 2px; margin-bottom:2px; }"
                "#ccert { margin-left: 5px; }"
                "#issuer { margin-left: 5px; }"
                "tt { font-size: small; }"
                );

    return style;
}
#define QL(x) QLatin1String(x)

bool SslErrorDialog::checkFailingCertsKnown( const QList<QSslError> &errors )
{
    // check if unknown certs caused errors.
    _unknownCerts.clear();

    QStringList errorStrings;

    QList<QSslCertificate> trustedCerts = _account->approvedCerts();

    for (int i = 0; i < errors.count(); ++i) {
        QSslError error = errors.at(i);
        if (trustedCerts.contains(error.certificate()) ||
            _unknownCerts.contains(error.certificate() )) {
            continue;
        }
        errorStrings += error.errorString();
        if (!error.certificate().isNull()) {
            _unknownCerts.append(error.certificate());
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
                msg += QL("<p>") + err.errorString() + QL("</p>");
            }
        }
        msg += QL("</div>");
        msg += certDiv( cert );
        if( _unknownCerts.count() > 1 ) {
            msg += QL("<hr/>");
        }
    }
    msg += QL("</div></body></html>");

    //qDebug() << "#  # # # # # ";
    //qDebug() << msg;
    QTextDocument *doc = new QTextDocument(0);
    QString style = styleSheet();
    doc->addResource( QTextDocument::StyleSheetResource, QUrl( QL("format.css") ), style);
    doc->setHtml( msg );

    _ui->_tbErrors->setDocument( doc );
    _ui->_tbErrors->show();

    return false;
}

QString SslErrorDialog::certDiv( QSslCertificate cert ) const
{
    QString msg;
    msg += QL("<div id=\"cert\">");
    msg += QL("<h3>") + tr("with Certificate %1").arg( Utility::escape(cert.subjectInfo( QSslCertificate::CommonName ))) + QL("</h3>");

    msg += QL("<div id=\"ccert\">");
    QStringList li;

    QString org = Utility::escape(cert.subjectInfo( QSslCertificate::Organization));
    QString unit = Utility::escape(cert.subjectInfo( QSslCertificate::OrganizationalUnitName));
    QString country = Utility::escape(cert.subjectInfo( QSslCertificate::CountryName));
    if (unit.isEmpty()) unit = tr("&lt;not specified&gt;");
    if (org.isEmpty()) org = tr("&lt;not specified&gt;");
    if (country.isEmpty()) country = tr("&lt;not specified&gt;");
    li << tr("Organization: %1").arg(org);
    li << tr("Unit: %1").arg(unit);
    li << tr("Country: %1").arg(country);
    msg += QL("<p>") + li.join(QL("<br/>")) + QL("</p>");

    msg += QL("<p>");

    QString md5sum = Utility::formatFingerprint(cert.digest(QCryptographicHash::Md5).toHex());
    QString sha1sum =  Utility::formatFingerprint(cert.digest(QCryptographicHash::Sha1).toHex());
    msg += tr("Fingerprint (MD5): <tt>%1</tt>").arg(md5sum) + QL("<br/>");
    msg += tr("Fingerprint (SHA1): <tt>%1</tt>").arg(sha1sum) + QL("<br/>");
    msg += QL("<br/>");
    msg += tr("Effective Date: %1").arg( cert.effectiveDate().toString()) + QL("<br/>");
    msg += tr("Expiration Date: %1").arg( cert.expiryDate().toString()) + QL("</p>");

    msg += QL("</div>" );

    msg += QL("<h3>") + tr("Issuer: %1").arg(Utility::escape(cert.issuerInfo( QSslCertificate::CommonName))) + QL("</h3>");
    msg += QL("<div id=\"issuer\">");
    li.clear();
    li << tr("Organization: %1").arg(Utility::escape(cert.issuerInfo( QSslCertificate::Organization)));
    li << tr("Unit: %1").arg(Utility::escape(cert.issuerInfo( QSslCertificate::OrganizationalUnitName)));
    li << tr("Country: %1").arg(Utility::escape(cert.issuerInfo( QSslCertificate::CountryName)));
    msg += QL("<p>") + li.join(QL("<br/>")) + QL("</p>");
    msg += QL("</div>" );
    msg += QL("</div>" );

    return msg;
}

bool SslErrorDialog::trustConnection()
{
    if( _allTrusted ) return true;

    bool stat = ( _ui->_cbTrustConnect->checkState() == Qt::Checked );
    qDebug() << "SSL-Connection is trusted: " << stat;

    return stat;
}

} // end namespace

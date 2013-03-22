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
#ifndef SSLERRORDIALOG_H
#define SSLERRORDIALOG_H

#include <QtCore>
#include <QDialog>
#include <QSslCertificate>
#include <QList>

#include "ui_sslerrordialog.h"

class QSslError;


namespace Mirall
{

class SslErrorDialog : public QDialog, public Ui::sslErrorDialog
{
    Q_OBJECT
public:
    explicit SslErrorDialog(QWidget *parent = 0);
    
    bool setErrorList( QList<QSslError> errors );

    bool trustConnection();

    void setCustomConfigHandle( const QString& );

signals:
    
public slots:
    void accept();

private:
    QString styleSheet() const;
    bool _allTrusted;

    QString certDiv( QSslCertificate ) const;

    QList<QSslCertificate> _unknownCerts;
    QString                _customConfigHandle;
};
} // end namespace

#endif // SSLERRORDIALOG_H

/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QList>
#include <QRegExp>
#include <QString>
#include <QSslCertificate>
#include <QSslConfiguration>

#include <QDebug>

#include "creds/credentialscommon.h"

#include "utility.h"
#include "account.h"
#include "syncengine.h"

namespace OCC
{

int handleNeonSSLProblems(const char* prompt,
                          char* buf,
                          size_t /*len*/,
                          int /*echo*/,
                          int /*verify*/,
                          void* userdata)
{
    int re = 0;
    const QString qPrompt = QString::fromLatin1( prompt ).trimmed();
    SyncEngine* engine = reinterpret_cast<SyncEngine*>(userdata);

    if( qPrompt.startsWith( QLatin1String("There are problems with the SSL certificate:"))) {
        // SSL is requested. If the program came here, the SSL check was done by Qt
        // It needs to be checked if the  chain is still equal to the one which
        // was verified by the user.
        const QRegExp regexp("fingerprint: ([\\w\\d:]+)");
        bool certOk = false;
        int pos = 0;
        // This is the set of certificates which QNAM accepted, so we should accept
        // them as well
        QList<QSslCertificate> certs = engine->account()->sslConfiguration().peerCertificateChain();

        while (!certOk && (pos = regexp.indexIn(qPrompt, 1+pos)) != -1) {
            QString neon_fingerprint = regexp.cap(1);

            foreach( const QSslCertificate& c, certs ) {
                QString verified_shasum = Utility::formatFingerprint(c.digest(QCryptographicHash::Sha1).toHex());
                qDebug() << "SSL Fingerprint from neon: " << neon_fingerprint << " compared to verified: " << verified_shasum;
                if( verified_shasum == neon_fingerprint ) {
                    certOk = true;
                    break;
                }
            }
        }
        // certOk = false;     DEBUG setting, keep disabled!
        if( !certOk ) { // Problem!
            qstrcpy( buf, "no" );
            re = -1;
        } else {
            qstrcpy( buf, "yes" ); // Certificate is fine!
        }
    } else {
        qDebug() << "Unknown prompt: <" << prompt << ">";
        re = -1;
    }
    return re;
}

} // namespace OCC

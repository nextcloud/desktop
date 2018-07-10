/****************************************************************************
** QWebDAV Library (qwebdavlib) - LGPL v2.1
**
** HTTP Extensions for Web Distributed Authoring and Versioning (WebDAV)
** from June 2007
**      http://tools.ietf.org/html/rfc4918
**
** Web Distributed Authoring and Versioning (WebDAV) SEARCH
** from November 2008
**      http://tools.ietf.org/html/rfc5323
**
** Missing:
**      - LOCK support
**      - process WebDAV SEARCH responses
**
** Copyright (C) 2012 Martin Haller <martin.haller@rebnil.com>
** for QWebDAV library (qwebdavlib) version 1.0
**      https://github.com/mhaller/qwebdavlib
**
** Copyright (C) 2012 Timo Zimmermann <meedav@timozimmermann.de>
** for portions from QWebdav plugin for MeeDav (LGPL v2.1)
**      http://projects.developer.nokia.com/meedav/
**
** Copyright (C) 2009-2010 Corentin Chary <corentin.chary@gmail.com>
** for portions from QWebdav - WebDAV lib for Qt4 (LGPL v2.1)
**      http://xf.iksaif.net/dev/qwebdav.html
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** for naturalCompare() (LGPL v2.1)
**      http://qt.gitorious.org/qt/qt/blobs/4.7/src/gui/dialogs/qfilesystemmodel.cpp
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
**
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB.  If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
** http://www.gnu.org/licenses/lgpl-2.1-standalone.html
**
****************************************************************************/

#ifndef QWEBDAVFILE_H
#define QWEBDAVFILE_H

//////#include "qwebdav_global.h"

#include <QDateTime>

class QWebdavItem
{
public:
    QWebdavItem();

    QWebdavItem(const QString &path, const QString &name,
                const QString &ext, bool dirOrFile,
                const QDateTime &lastModified, quint64 size);

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    QWebdavItem(const QString &path, const QString &name,
                const QString &ext, bool dirOrFile,
                const QDateTime &lastModified, quint64 size,
                const QString &displayName, const QDateTime &createdAt,
                const QString &contentLanguage, const QString &entityTag,
                const QString &mimeType, bool isExecutable,
                const QString &source);
#endif

    bool isDir() const;
    QString path() const;
    QString name() const;
    QString ext() const;
    QDateTime lastModified() const;
    QString lastModifiedStr() const;
    quint64 size() const;

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    QString displayName() const;
    QDateTime createdAt() const;
    QString createdAtStr() const;
    QString contentLanguage() const;
    QString entityTag() const;
    QString mimeType() const;
    bool isExecutable() const;
    QString source() const;
#endif

    bool operator <(const QWebdavItem &other) const;

protected:
    bool m_dirOrFile;
    QString m_path;
    QString m_name;
    QString m_ext;
    QDateTime m_lastModified;
    QString m_lastModifiedStr;
    quint64 m_size;

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    QString m_displayName;
    QDateTime m_createdAt;
    QString m_createdAtStr;
    QString m_contentLanguage;
    QString m_entityTag;
    QString m_mimeType;
    bool m_isExecutable;
    QString m_source;
#endif

};

#endif // QWEBDAVFILE_H

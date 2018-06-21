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

#include "qwebdavitem.h"
#include "qnaturalsort.h"

QWebdavItem::QWebdavItem() :
    m_dirOrFile()
   ,m_path()
   ,m_name()
   ,m_ext()
   ,m_lastModified()
   ,m_lastModifiedStr()
   ,m_size(0)
#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
   ,m_displayName()
   ,m_createdAt()
   ,m_createdAtStr()
   ,m_contentLanguage()
   ,m_entityTag()
   ,m_mimeType()
   ,m_isExecutable(false)
   ,m_source()
 #endif // QWEBDAVITEM_EXTENDED_PROPERTIES
{
}

QWebdavItem::QWebdavItem(const QString &path, const QString &name,
                         const QString &ext, bool dirOrFile,
                         const QDateTime &lastModified, quint64 size) :
   m_dirOrFile(dirOrFile)
  ,m_path(path)
  ,m_name(name)
  ,m_ext(ext)
  ,m_lastModified(lastModified)
  ,m_lastModifiedStr(lastModified.toString("yyyy-MM-dd hh:mm")) // ISO format
  ,m_size(size)
#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
  ,m_displayName()
  ,m_createdAt()
  ,m_createdAtStr()
  ,m_contentLanguage()
  ,m_entityTag()
  ,m_mimeType()
  ,m_isExecutable(false)
  ,m_source()
#endif // QWEBDAVITEM_EXTENDED_PROPERTIES
{
}

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
QWebdavItem::QWebdavItem(const QString &path, const QString &name,
            const QString &ext, bool dirOrFile,
            const QDateTime &lastModified, quint64 size,
            const QString &displayName, const QDateTime &createdAt,
            const QString &contentLanguage, const QString &entityTag,
            const QString &mimeType, bool isExecutable,
            const QString &source) :
    m_dirOrFile(dirOrFile)
   ,m_path(path)
   ,m_name(name)
   ,m_ext(ext)
   ,m_lastModified(lastModified)
   ,m_lastModifiedStr(lastModified.toString("yyyy-MM-dd hh:mm")) // ISO format
   ,m_size(size)
   ,m_displayName(displayName)
   ,m_createdAt(createdAt)
   ,m_createdAtStr(createdAt.toString("yyyy-MM-dd hh:mm")) // ISO format
   ,m_contentLanguage(contentLanguage)
   ,m_entityTag(entityTag)
   ,m_mimeType(mimeType)
   ,m_isExecutable(isExecutable)
   ,m_source(source)
{
}
#endif // QWEBDAVITEM_EXTENDED_PROPERTIES

bool QWebdavItem::isDir() const
{
    return m_dirOrFile;
}

QString QWebdavItem::path() const
{
    return m_path;
}

QString QWebdavItem::name() const
{
    return m_name;
}

QString QWebdavItem::ext() const
{
    return m_ext;
}

QDateTime QWebdavItem::lastModified() const
{
    return m_lastModified;
}

QString QWebdavItem::lastModifiedStr() const
{
    return m_lastModifiedStr;
}

quint64 QWebdavItem::size() const
{
    return m_size;
}

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
QString QWebdavItem::displayName() const
{
    return m_displayName;
}

QDateTime QWebdavItem::createdAt() const
{
    return m_createdAt;
}

QString QWebdavItem::createdAtStr() const
{
    return m_createdAtStr;
}
QString QWebdavItem::contentLanguage() const
{
    return m_contentLanguage;
}

QString QWebdavItem::entityTag() const
{
    return m_entityTag;
}

QString QWebdavItem::mimeType() const
{
    return m_mimeType;
}

bool QWebdavItem::isExecutable() const
{
    return m_isExecutable;
}

QString QWebdavItem::source() const
{
    return m_source;
}
#endif // QWEBDAVITEM_EXTENDED_PROPERTIES

bool QWebdavItem::operator <(const QWebdavItem &other) const
{
    if(m_dirOrFile != other.isDir())
        return m_dirOrFile;

    // sort with simple QString comparison, e.g. 1 10 2 200 6 600
    //return m_name.toLower() < other.name().toLower();

    // natural sort e.g. 1 2 6 10 200 600
    return QNaturalSort::naturalCompare(m_name.toLower(), other.name().toLower() ) < 0;
}



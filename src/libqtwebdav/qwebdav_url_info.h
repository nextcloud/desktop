/* This file is part of QWebdav
 *
 * Copyright (C) 2009-2010 Corentin Chary <corentin.chary@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef QWEBDAV_URL_INFO_H
#define QWEBDAV_URL_INFO_H

#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QDateTime>
#include <QUrlInfo>
#include <QVariant>

#include "qwebdav_export.h"
#include "qwebdav.h"

class QWEBDAV_EXPORT QWebdavUrlInfo : virtual public QUrlInfo
{
 public:
  QWebdavUrlInfo ();
  QWebdavUrlInfo ( const QDomElement & dom );
  QWebdavUrlInfo ( const QWebdavUrlInfo & wui );

  virtual ~QWebdavUrlInfo ();
 private:
  int codeFromResponse( const QString & response );
  QDateTime parseDateTime( const QString& input, const QString& type );
  void davParsePropstats(const QString & path, const QDomNodeList & propstat);

 public:
  void setCreatedAt(const QDateTime & date);
  void setDisplayName(const QString & name);
  void setSource(const QString & source);
  void setContentLanguage(const QString & lang);
  void setEntitytag(const QString & etag);
  void setMimeType(const QString & mime);

  QDateTime createdAt() const;
  QString displayName() const;
  QString source() const;
  QString contentLanguage() const;
  QString entityTag() const;
  QString mimeType() const;

  QDomElement propElement() const;
  const QWebdav::PropValues & properties() const;
 private:

  QDomNode node_;
  QWebdav::PropValues properties_;
  QDateTime createdAt_;
  QString displayName_;
  QString source_;
  QString contentLanguage_;
  QString entityTag_;
  QString mimeType_;
};

#endif /* QWEBDAV_URL_INFO_H */

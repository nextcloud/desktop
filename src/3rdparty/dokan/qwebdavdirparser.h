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

#ifndef QWEBDAVDIRPARSER_H
#define QWEBDAVDIRPARSER_H

// without GUI
// #include <QtCore>
//#include <QApplication>

/*#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
*/

#include "C:\\Qt\\qt-5.6.2\\qtbase\\INCLUDE\\QTXML\\QDOMDOCUMENT"
#include "C:\\Qt\\qt-5.6.2\\qtbase\\INCLUDE\\QTXML\\QDOMELEMENT"
#include "C:\\Qt\\qt-5.6.2\\qtbase\\INCLUDE\\QTXML\\QDOMNODELIST"

//#include "qwebdav_global.h"

#include "qwebdav.h"
#include "qwebdavitem.h"

class QWebdavDirParser : public QObject
{
    Q_OBJECT

public:
    QWebdavDirParser(QObject *parent = 0);
    ~QWebdavDirParser();

    //! get all items of a collection
    bool listDirectory(QWebdav *pWebdav, const QString &path);
    //! get only information about the collection
    bool getDirectoryInfo(QWebdav *pWebdav, const QString &path);
    //! get only information about a file
    bool getFileInfo(QWebdav *pWebdav, const QString &path);
    bool listItem(QWebdav *pWebdav, const QString &path);

    QList<QWebdavItem> getList();
    bool isBusy() const;
    bool isFinished() const;
    QString path() const;

signals:
    void finished();
    void errorChanged(QString);

public slots:
    void abort();
	void slotError(QNetworkReply::NetworkError e);

protected slots:
    //void error(QNetworkReply::NetworkError code);
    void replyFinished();
    void replyDeleteLater(QNetworkReply* reply);

protected:
    void parseMultiResponse( const QByteArray &data );
    void parseResponse( const QDomElement& dom );
    void davParsePropstats (const QString &path, const QDomNodeList &propstats);
    int codeFromResponse( const QString &response );
    QDateTime parseDateTime( const QString &input, const QString &type);

private:
    QScopedPointer<QMutex> m_mutex;
    QWebdav *m_webdav;
    QNetworkReply *m_reply;
    QList<QWebdavItem> m_dirList;
    QString m_path;
    bool m_includeRequestedURI;
    bool m_busy;
    bool m_abort;
};

#endif // QWEBDAVDIRPARSER_H

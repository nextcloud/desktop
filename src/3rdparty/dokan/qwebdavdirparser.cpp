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

#include "qwebdavdirparser.h"

QWebdavDirParser::QWebdavDirParser(QObject *parent) : QObject(parent)
  ,m_webdav(0)
  ,m_reply(0)
  ,m_path()
  ,m_includeRequestedURI(false)
  ,m_busy(false)
  ,m_abort(false)
{
	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::QWebdavDirParser");
	fclose(fp);

    m_mutex.reset(new QMutex(QMutex::Recursive));
}

QWebdavDirParser::~QWebdavDirParser()
{
	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::~QWebdavDirParser()");
	fclose(fp);

    if (m_reply!=0) {
        m_reply->deleteLater();
        m_reply = 0;
    }
}

bool QWebdavDirParser::listDirectory(QWebdav *pWebdav, const QString &path)
{

	std::string m_alias = path.toLocal8Bit().constData();
	///////////////////fprintf(fp, "m_alias: %s, ", m_alias.c_str())

	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n QWebdavDirParser::listDirectory  INIT 1,  path: %s", m_alias.c_str());
	fclose(fp);

	if (m_busy)
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "m_busy");
		fclose(fp);

		return false;
	}



	if (m_reply != 0)
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "m_reply != 0");
		fclose(fp);

		return false;
	}



	if (pWebdav == 0)
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "pWebdav == 0");
		fclose(fp);
		return false;
	}




	if (path.isEmpty())
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "path.isEmpty()");
		fclose(fp);

		return false;
	}



	if (!path.endsWith("/"))
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "!path.endsWith()");
		fclose(fp);

		return false;
	}




    m_webdav = pWebdav;
    m_path = path;
    m_busy = true;
    m_abort = false;
    m_includeRequestedURI = false;

    m_reply = pWebdav->list(path);
    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));
	connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(slotError(QNetworkReply::NetworkError)));
	
	if (m_reply->error() == QNetworkReply::UnknownNetworkError)
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nQWebdavDirParser::listDirectory  INIT 2  --> Error: %s", "NetworkReply::UnknownNetworkError");
		fclose(fp);

	}
	std::string m_Error = m_reply->errorString().toLocal8Bit().constData();
	///////////////////fprintf(fp, "m_alias: %s, ", m_alias.c_str())


		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\nQWebdavDirParser::listDirectory  INIT 2  --> Error: %s", m_Error.c_str());
	fclose(fp);

	if (!m_dirList.isEmpty())
	{

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "!m_dirList.isEmpty");
		fclose(fp);


		m_dirList.clear();
	}
	else
	{


	}

	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::listDirectory  INIT 3");
	fclose(fp);



    return true;
}


void QWebdavDirParser::slotError(QNetworkReply::NetworkError e) {
	
	char wwwr[50];
	sprintf(wwwr, "%d", e);
	FILE* fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n-------------------ERROR: %s", wwwr);
	fclose(fp);

	qDebug() << "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq slotError" << e;
}





bool QWebdavDirParser::listItem(QWebdav *pWebdav, const QString &path)
{


	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::listItem");
	fclose(fp);

    if (m_busy)
        return false;

    if (m_reply!=0)
        return false;

    if (pWebdav==0)
        return false;

    if (path.isEmpty())
        return false;

    m_webdav = pWebdav;
    m_path = path;
    m_busy = true;
    m_includeRequestedURI = true;

    m_reply = pWebdav->list(path, 0);

	if (!m_dirList.isEmpty())
	{
		m_dirList.clear();
	}
	else
	{


	}

    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));


    return true;
}

bool QWebdavDirParser::getDirectoryInfo(QWebdav *pWebdav, const QString &path)
{


	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::getDirectoryInfo");
	fclose(fp);

    if (!path.endsWith("/"))
        return false;

    return listItem(pWebdav, path);
}

bool QWebdavDirParser::getFileInfo(QWebdav *pWebdav, const QString &path)
{
	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::getFileInfo");
	fclose(fp);

    if (path.endsWith("/"))
        return false;

    return listItem(pWebdav, path);
}

QList<QWebdavItem> QWebdavDirParser::getList()
{


	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::getList()");
	fclose(fp);

    return m_dirList;
}

bool QWebdavDirParser::isBusy() const
{
	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::isBusy()");
	fclose(fp);

    return m_busy;
}

bool QWebdavDirParser::isFinished() const
{

	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::isFinished()");
	fclose(fp);

    if (m_reply!=0)
        return m_reply->isFinished();
    else
        return true;
}

QString QWebdavDirParser::path() const
{

    return m_path;
}

void QWebdavDirParser::abort()
{

	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::abort()");
	fclose(fp);

    m_abort = true;

    if (m_reply!=0)
        m_reply->abort();

    m_reply = 0;
    m_busy = false;
}

void QWebdavDirParser::replyFinished()
{

	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::replyFinished() 1");
	fclose(fp);


    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());

#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdavDirParser::replyFinished()";
#endif

    if (m_reply!=reply) {
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "QWebdavDirParser::replyFinished()  wrong reply : m_reply!=reply");
		fclose(fp);

#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdavDirParser::replyFinished()  wrong reply : m_reply!=reply";
#endif
	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "invokeMethod replyDeleteLater INIT A");
	fclose(fp);

        QMetaObject::invokeMethod(this,"replyDeleteLater", Qt::QueuedConnection, Q_ARG(QNetworkReply*, reply));

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "invokeMethod replyDeleteLater END A");
		fclose(fp);

        return;
    }

	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::replyFinished() 2");
	fclose(fp);


	if (m_abort)
	{
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "m_abort");
		fclose(fp);

		return;
	}

    {
        QMutexLocker locker(m_mutex.data());

        QString contentType = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
    #ifdef DEBUG_WEBDAV
        qDebug() << "   Reply finished. Content header:" << contentType;
    #endif
        if ( (m_reply->error() != QNetworkReply::NoError) && (m_reply->error() != QNetworkReply::OperationCanceledError) ) {
            QString errStr = m_reply->errorString();
            errStr = errStr.right(errStr.size()-errStr.indexOf("server replied:")+1);
            emit errorChanged(errStr);

			FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
			fprintf(fp, "\n %s", "Reply has error");
			fclose(fp);
    #ifdef DEBUG_WEBDAV
            qDebug() << "   Reply has error. Error:" << m_reply->errorString() << "Code:" << m_reply->error();
    #endif
        }
        else {

			FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
			fprintf(fp, "\n %s", "QWebdavDirParser::replyFinished() 3");
			fclose(fp);

            QByteArray data = m_reply->readAll();
            //        if(data.isEmpty()) {
            //            qDebug() << "QWebdavDirParser::replyFinished() | Reply has no data."; //<< m_reply->rawHeaderPairs();
            //        }

            if(contentType.contains("xml")) {
                // DEBUG
                //qDebug() << data;
                parseMultiResponse(data);
            }
        }

        m_reply = 0;
    }


	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "invokeMethod replyDeleteLater INIT B");
	fclose(fp);

    QMetaObject::invokeMethod(this,"replyDeleteLater", Qt::QueuedConnection, Q_ARG(QNetworkReply*, reply));

	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "invokeMethod replyDeleteLater END B");
	fclose(fp);

}

void QWebdavDirParser::replyDeleteLater(QNetworkReply* reply)
{
	FILE * fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::replyDeleteLater INIT 1");
	fclose(fp);


	if (reply == 0)
	{
		FILE * fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "reply == 0");
		fclose(fp);

		return;
	}

#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdavDirParser::replyDeleteLater()   reply->url == " << reply->url().toString(QUrl::RemoveUserInfo);
    qDebug() << "QWebdavDirParser::replyDeleteLater()      reply->isFinished() == " << reply->isFinished();
    qDebug() << "QWebdavDirParser::replyDeleteLater()      reply->bytesAvailable() == " << reply->bytesAvailable();
    qDebug() << "QWebdavDirParser::replyDeleteLater()      reply->bytesToWrite() == " << reply->bytesToWrite();
#endif

    if ((!reply->isFinished()) || reply->bytesToWrite() || reply->bytesAvailable()) {
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdavDirParser::replyDeleteLater()      reinvoke replyDeleteLater()";
#endif
        reply->readAll();
        reply->close();
        QMetaObject::invokeMethod(this,"replyDeleteLater", Qt::QueuedConnection, Q_ARG(QNetworkReply*, reply));

		FILE * fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n %s", "reply == 0");
		fclose(fp);

        return;
    }

    {
        //QMutexLocker locker(m_mutex.data());
        disconnect(reply, 0, 0, 0);
        reply->deleteLater();
        m_busy = false;
    }

	
	fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "emit finished");
	fclose(fp);

    emit finished();



#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdavDirParser::replyDeleteLater()      reply->deleteLater() done";
#endif

}

void QWebdavDirParser::parseMultiResponse(const QByteArray &data)
{

	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::parseMultiResponse");
	fclose(fp);

    if (m_abort)
        return;

    QDomDocument multiResponse;
    multiResponse.setContent(data, true);

    for(QDomNode n = multiResponse.documentElement().firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (m_abort)
            return;

        QDomElement thisResponse = n.toElement();

        if(thisResponse.isNull())
            continue;

        QString responseName = QUrl::fromPercentEncoding(thisResponse.namedItem("href").toElement().text().toUtf8());
        if(responseName.isEmpty())
            continue;

        // ingore the path itself within the listing of the path

        // Apache returns only path without scheme and authority
        if ((!m_includeRequestedURI) && ((m_webdav->rootPath()+m_path) == responseName))
            continue;

        // MS IIS returns URL
        if ((!m_includeRequestedURI) && responseName.startsWith("http")) {
            QUrl checkUrl(responseName);
            if ((m_webdav->rootPath()+m_path) == checkUrl.path())
                continue;
        }

        if (m_abort)
            return;

        parseResponse(thisResponse);

        // container without slash at the end is a wrong answer
        // remove this item from the list
        if ((!m_includeRequestedURI) && m_dirList.last().isDir() && !responseName.endsWith("/")) {

            if (responseName.startsWith("http")) {
                // box.com
                QUrl checkUrl(responseName);
                if ((m_webdav->rootPath()+m_path) == checkUrl.path()+"/")
                    m_dirList.removeLast();
            } else {
                // dav-pocket.appspot.com
                if ((m_webdav->rootPath()+m_path) == (responseName+"/"))
                    m_dirList.removeLast();
            }
        }

    }

    qSort(m_dirList.begin(), m_dirList.end());
}

void QWebdavDirParser::parseResponse(const QDomElement &dom)
{
	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::parseResponse");
	fclose(fp);

    if (m_abort)
        return;

    QDomElement href = dom.namedItem("href").toElement();
    if (href.isNull()) return;

    QString urlStr = QUrl::fromPercentEncoding(href.text().toUtf8());
    QDomNodeList propstats = dom.elementsByTagName("propstat");
    davParsePropstats(urlStr, propstats);
}

void QWebdavDirParser::davParsePropstats(const QString &path, const QDomNodeList &propstats)
{


	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::davParsePropstats");
	fclose(fp);

    if (m_abort)
        return;


    QString path_;
    QString name;
    QString ext;
    bool dirOrFile = false;
    QDateTime lastModified;
    quint64 size = 0;

    if (path.startsWith("http")) { // with scheme and authority
        QUrl pathUrl(path);
        path_ = pathUrl.path();
    } else
        path_ = path; // without scheme and authority

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    QString displayName;
    QDateTime createdAt;
    QString contentLanguage;
    QString entityTag;
    QString mimeType;
    bool isExecutable;
    QString source;
#endif

    // name
    QStringList pathElements = path_.split('/', QString::SkipEmptyParts);
    name = pathElements.isEmpty() ? "/" : pathElements.back();

    for ( int i = 0; i < propstats.count(); i++) {
        QDomElement propstat = propstats.item(i).toElement();
        QDomElement status = propstat.namedItem( "status" ).toElement();

        if ( status.isNull() ) {
			FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
			fprintf(fp, "\n %s", "Error, no status code in this propstat");
			fclose(fp);

#ifdef DEBUG_WEBDAV

            qDebug() << "Error, no status code in this propstat";
#endif
            return;
        }

        if (m_abort)
            return;

        int code = codeFromResponse( status.text() );

        if (code == 404) // property not available
            continue;

        QDomElement prop = propstat.namedItem( "prop" ).toElement();

        if ( prop.isNull() ) {
			FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
			fprintf(fp, "\n %s", "Error: no prop segment in this propstat.");
			fclose(fp);

#ifdef DEBUG_WEBDAV
            qDebug() << "Error: no prop segment in this propstat.";
#endif
            return;
        }

        for ( QDomNode n = prop.firstChild(); !n.isNull(); n = n.nextSibling() ) {
            QDomElement property = n.toElement();

            if (m_abort)
                return;

            if (property.isNull())
                continue;

            if ( property.namespaceURI() != "DAV:" ) {
                // parse only DAV namespace properties
                continue;
            }

            if ( property.tagName() == "getcontentlength" )
                size = property.text().toULongLong();
            else if ( property.tagName() == "getlastmodified" )
                lastModified = parseDateTime( property.text(), property.attribute("dt") );
            else if ( property.tagName() == "resourcetype" )
            {
                if ( !property.namedItem( "collection" ).toElement().isNull() )
                    dirOrFile = true;
            }
#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
            else
                if ( property.tagName() == "creationdate" )
                    createdAt = parseDateTime( property.text(), property.attribute("dt") );
                else if ( property.tagName() == "displayname" )
                    displayName = property.text();
                else if ( property.tagName() == "source" )
                {
                    QDomElement sourceElement;
                    sourceElement = property.namedItem( "link" ).toElement().namedItem( "dst" ).toElement();

                    if ( !sourceElement.isNull() )
                        source = sourceElement.text();
                }
                else if ( property.tagName() == "getcontentlanguage" )
                    contentLanguage = property.text();
                else if ( property.tagName() == "getcontenttype" )
                {
                    if ( property.text() == "httpd/unix-directory" )
                        dirOrFile = true;

                    mimeType = property.text();
                }
                else if ( property.tagName() == "executable" )
                {
                    if ( property.text() == "T" )
                        isExecutable = true;
                }
                else if ( property.tagName() == "getetag" )
                    entityTag = property.text();
#endif
#ifdef DEBUG_WEBDAV
                else
                    qDebug() << "Found unknown WEBDAV property: " << property.tagName() << property.text();
#endif
        }
    }

    // check directory path
    if (dirOrFile && !path_.endsWith("/"))
        path_.append("/");

    // get file extension
    if (!dirOrFile) {
        int fileTypeSepIdx = name.lastIndexOf(".");
        if (fileTypeSepIdx == -1) {
            ext = "";
        } else {
            ext = name.right(name.size() - fileTypeSepIdx - 1).toUpper();
        }
    }

    path_.remove(0,m_webdav->rootPath().size());

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    m_dirList.append(QWebdavItem(path_, name,
                                 ext, dirOrFile,
                                 lastModified, size,
                                 displayName, createdAt,
                                 contentLanguage, entityTag,
                                 mimeType, isExecutable,
                                 source));
#else
    m_dirList.append(QWebdavItem(path_, name,
                                 ext, dirOrFile,
                                 lastModified, size));
#endif
}

int QWebdavDirParser::codeFromResponse( const QString &response )
{


    int firstSpace = response.indexOf( ' ' );
    int secondSpace = response.indexOf( ' ', firstSpace + 1 );
    return response.mid( firstSpace + 1, secondSpace - firstSpace - 1 ).toInt();
}

QDateTime QWebdavDirParser::parseDateTime(const QString &input, const QString &type)
{


	FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
	fprintf(fp, "\n %s", "QWebdavDirParser::parseDateTime");
	fclose(fp);

    QDateTime datetime;
    QLocale usLocal(QLocale::English, QLocale::UnitedStates);

    if ( type == "dateTime.tz" )
        datetime =  QDateTime::fromString(input, Qt::ISODate );
    else if ( type == "dateTime.rfc1123" )
        datetime = usLocal.toDateTime( input );

    if (datetime.isValid())
        return datetime;

    datetime = usLocal.toDateTime(input.left(25), "ddd, dd MMM yyyy hh:mm:ss");
    if (datetime.isValid())
        return datetime;
    datetime = usLocal.toDateTime(input.left(19), "yyyy-MM-ddThh:mm:ss");
    if (datetime.isValid())
        return datetime;
    datetime = usLocal.toDateTime(input.mid(5, 20) , "d MMM yyyy hh:mm:ss");
    if (datetime.isValid())
        return datetime;
    QDate date;
    QTime time;

    date = usLocal.toDate(input.mid(5, 11) , "d MMM yyyy");
    time = usLocal.toTime(input.mid(17, 8) , "hh:mm:ss");
    datetime = QDateTime(date, time);

#ifdef DEBUG_WEBDAV
    if(!datetime.isValid())
        qDebug() << "QWebdavDirParser::parseDateTime() | Unknown date time format:" << input;
#endif

    return datetime;
}

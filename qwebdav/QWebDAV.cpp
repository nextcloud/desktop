/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync.
 *
 *    owncloud_sync is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    owncloud_sync is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with owncloud_sync.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "QWebDAV.h"

// Qt Standard Includes
#include <QDebug>
#include <QBuffer>
#include <QDateTime>

// Qt Network Includes
#include <QNetworkReply>
#include <QAuthenticator>
#include <QNetworkRequest>
#include <QSslError>

// Qt's XML Includes
#include <QDomDocument>
#include <QDomElement>

qint64 QWebDAV::mRequestNumber = 0;

QWebDAV::QWebDAV(QObject *parent) :
    QNetworkAccessManager(parent), mInitialized(false)
{
}

void QWebDAV::initialize(QString hostname, QString username, QString password,
                         QString pathFilter)
{
    mFirstAuthentication = true;
    mHostname = hostname;
    mUsername = username;
    mPassword = password;
    mPathFilter = pathFilter;
    mInitialized = true;

    connect(this,SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
            SLOT(slotAuthenticationRequired(QNetworkReply*, QAuthenticator*)));
    mInitialized = true;
}

QNetworkReply* QWebDAV::sendWebdavRequest(QUrl url, DAVType type,
                                          QByteArray verb, QIODevice *data,
                                          int depth)
{
    // Prepare the network request and headers
    QNetworkRequest request;
    QNetworkReply *reply;
    request.setUrl(url);
    request.setRawHeader(QByteArray("Host"),url.host().toUtf8());

    // First, find out what type we want
    if( type == DAVLIST ) {
        // A PROPFIND can include 0, 1 or infinity
        QString depthString;
        if ( depth < 0 ) {
            depthString = "0";
        } else if ( depth < 2 ) {
            depthString.append("%1").arg(depth);
        } else {
            depthString = "infinity";
        }
        request.setRawHeader(QByteArray("Depth"),
                             QByteArray(depthString.toAscii()));
        request.setAttribute(QNetworkRequest::User, QVariant("list"));
        request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+1)
                             ,QVariant(mRequestNumber));
        request.setRawHeader(QByteArray("Content-Type"),
                             QByteArray("text/xml; charset=\"utf-8\""));
        request.setRawHeader(QByteArray("Content-Length"),QByteArray("99999"));

        reply = sendCustomRequest(request,verb,data);
    } else if ( type == DAVGET ) {
        request.setRawHeader("User-Agent", "QWebDAV 0.1");
        request.setAttribute(QNetworkRequest::User, QVariant("get"));
        reply = QNetworkAccessManager::get(request);
        connect(reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                 this, SLOT(slotError(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                 this, SLOT(slotSslErrors(QList<QSslError>)));
    } else if ( type == DAVPUT )  {
        request.setAttribute(QNetworkRequest::User, QVariant("put"));
        request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+1)
                             ,QVariant(mRequestNumber));
        reply = QNetworkAccessManager::put(request,data);
    } else if ( type == DAVMKCOL ) {
        request.setAttribute(QNetworkRequest::User, QVariant("mkcol"));
        reply = sendCustomRequest(request,verb,0);
    } else if ( type == DAVDELETE ) {
        request.setAttribute(QNetworkRequest::User, QVariant("delete"));
        reply = sendCustomRequest(request, verb,0);
    } else {
        qDebug() << "Error! DAV Request of type " << type << " is not known!";
        reply = 0;
    }

    // Connect the finished() signal!
    connectReplyFinished(reply);
    return reply;
}

QNetworkReply* QWebDAV::list(QString dir, int depth )
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // This is the Url of the webdav server + the directory we want a listing of
    QUrl url(mHostname+dir);

    // Prepare the query. We want a listing of all properties the WebDAV
    // server is willing to provide
    mRequestNumber++;
    QByteArray *query = new QByteArray();
    *query += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
    *query += "<D:propfind xmlns:D=\"DAV:\">";
    *query += "<D:allprop/>";
    *query += "</D:propfind>";
    QBuffer *data = new QBuffer(query);
    QByteArray verb("PROPFIND");
    mRequestQueries[mRequestNumber] = query;
    mRequestData[mRequestNumber] = data;
    // Finally send this to the WebDAV server
    return sendWebdavRequest(url,DAVLIST,verb,data,depth);
}

void QWebDAV::slotReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    slotFinished(reply);
}

void QWebDAV::slotFinished(QNetworkReply *reply)
{
    if ( reply->error() != 0 ) {
        qDebug() << "WebDAV request returned error: " << reply->error()
                    << " On URL: " << reply->url().toString();
        //qDebug() << reply->readAll();
    }

    // Good, but what is it responding to? Find out:
    if ( reply->request().attribute(
                QNetworkRequest::User).toString().contains("list") ) {
        //qDebug() << "Oh a listing! How fun!!";
        processDirList(reply->readAll(),reply->url().path());
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("get") ) {
        //qDebug() << "Oh a GET! How fun!!";
        //qDebug() << "Get Data: "<< reply->readAll();
        processFile(reply);
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("put")) {
        //qDebug() << "Oh a PUT! How fun!!" <<
        //            reply->request().url().path().replace(mPathFilter,"");
        emit uploadComplete(
                    reply->request().url().path().replace(
                        QRegExp("^"+mPathFilter),""));
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("mkcol")) {
        emit directoryCreated(reply->request().url().path().replace(
                                  QRegExp("^"+mPathFilter),""));
        //
        // Do nothing for now
    } else {
        qDebug() << "Who knows what the server is trying to tell us.";
    }
    // Now check if additional data needs to be deleted!
    qint64 value = reply->request().attribute(
                QNetworkRequest::Attribute(
                    QNetworkRequest::User+1)).toLongLong();
    //qDebug() << "Request number: " << value;
    if(value > 0 ) {
        delete mRequestData.value(value);
        delete mRequestQueries.value(value);
        mRequestData.remove(value);
        mRequestQueries.remove(value);
    }

    reply->deleteLater();
}

void QWebDAV::slotAuthenticationRequired(QNetworkReply *reply,
                                         QAuthenticator *auth)
{
    //qDebug() << "Authenticating: ";
    if( mFirstAuthentication && auth ) {
        auth->setUser(mUsername);
        auth->setPassword(mPassword);
        mFirstAuthentication = false;
    }
}

void QWebDAV::dirList(QString dir)
{
    list(dir,1);
}

void QWebDAV::processDirList(QByteArray xml, QString url)
{
    QList<QWebDAV::FileInfo> list;
    QDomDocument domDocument;
    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!domDocument.setContent(xml, true, &errorStr, &errorLine,
                                &errorColumn)) {
        qDebug() << "Error at line " << errorLine << " column " << errorColumn;
        qDebug() << errorStr;
        emit directoryListingError(url);
        return;
    }

    QDomElement root = domDocument.documentElement();
    if( root.tagName() != "multistatus" ) {
        qDebug() << "Badly formatted XML!" << xml;
        emit directoryListingError(url);
        return;
    }

    QString name;
    QString size;
    QString last;
    QString type;
    QString available;
    QDomElement response = root.firstChildElement("response");
    while (!response.isNull()) {
        // Parse first response
        QDomElement child = response.firstChildElement();
        while (!child.isNull()) {
            //qDebug() << "ChildName: " << child.tagName();
            if ( child.tagName() == "href" ) {
                name = child.text();
            } else if ( child.tagName() == "propstat") {
                QDomElement prop = child.firstChildElement("prop")
                        .firstChildElement();
                while(!prop.isNull()) {
                    //qDebug() << "PropName: " << prop.tagName();
                    if( prop.tagName() == "getlastmodified") {
                        last = prop.text();
                    } else if ( prop.tagName() == "getcontentlength" ||
                                prop.tagName() == "quota-used-bytes") {
                        size = prop.text();
                    } else if ( prop.tagName() == "quota-available-bytes") {
                        available = prop.text();
                    } else if ( prop.tagName() == "resourcetype") {
                        QDomElement resourseType = prop.firstChildElement("");
                        type = resourseType.tagName();
                    }

                    prop = prop.nextSiblingElement();
                }
            }
            child = child.nextSiblingElement();
        }
//        qDebug() << "Name: " << name << "\nSize: " << size << "\nLastModified: "
//                 << last << "\nSizeAvailable: " << available << "\nType: "
//                 << type << "\n";
        // Filter out the requested directory from this list
        //qDebug() << "Type: " << type << "Name: " << name << " URL: " << url;
        name = QUrl::fromPercentEncoding(name.toAscii());
        if( !(type == "collection" && name == url) ) {
            // Filter out the pathname from the filename and decode URL
            name.replace(mPathFilter,"");

            // Store lastmodified as an EPOCH format
            last.replace(" +0000","");
            last.replace(",","");
            QDateTime date = QDateTime::fromString(last,
                                                   "ddd dd MMM yyyy HH:mm:ss");
            date.setTimeSpec(Qt::UTC);
            last = QString("%1").arg(date.toMSecsSinceEpoch());
            list.append(QWebDAV::FileInfo(name,last,size.toLongLong(),
                                          available.toLongLong(),type));
        }
        name = size = last = type = available = "";
        response = response.nextSiblingElement();
    }
    //for(int i = 0; i < list.size(); i++ ) {
    //    list[i].print();
    //}

    // Let whoever is listening know that we have their stuff ready!
    emit directoryListingReady(list);
}

QNetworkReply* QWebDAV::get(QString fileName)
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // This is the Url of the webdav server + the file we want to get
    QUrl url(mHostname+fileName);

    // Finally send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(url,DAVGET);
    //qDebug() << "GET REPLY: " << reply->readAll();
    return reply;
}

QNetworkReply* QWebDAV::put(QString fileName, QByteArray data)
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // This is the Url of the webdav server + the file we want to get
    QUrl url(mHostname+fileName);

    // Encapsulate data in an QIODevice
    mRequestNumber++;
    QByteArray *safeData = new QByteArray(data);
    QBuffer *buffer = new QBuffer(safeData);
    mRequestQueries[mRequestNumber] = safeData;
    mRequestData[mRequestNumber] = buffer;

    // Finally send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(url,DAVPUT,0,buffer);
    //qDebug() << "PUT REPLY: " << reply->readAll();
    return reply;
}

QNetworkReply* QWebDAV::mkdir(QString dirName)
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // This is the URL of the webdav server + the file we want to get
    QUrl url(mHostname+dirName);

    // Finally send this to the WebDAV server
    QByteArray verb("MKCOL");
    QNetworkReply *reply = sendWebdavRequest(url,DAVMKCOL,verb);
    //qDebug() << "MKCOL REPLY: " << reply->readAll();
    return reply;
}

void QWebDAV::slotReadyRead()
{
    //qDebug() << "Data ready to be read!";
}

void QWebDAV::slotSslErrors(QList<QSslError> errorList)
{

}

void QWebDAV::slotError(QNetworkReply::NetworkError error)
{
}

void QWebDAV::processFile(QNetworkReply* reply)
{
    // Remove all the WebDAV paths and just leave the base names
    QString fileName = reply->request().url().path().replace(mPathFilter,"")
            .replace("\%20"," ");

    //qDebug() << "File Ready: " << fileName;
    emit fileReady(reply->readAll(),fileName);
}

void QWebDAV::connectReplyFinished(QNetworkReply *reply)
{
    connect(reply, SIGNAL(finished ()),
            this, SLOT(slotReplyFinished ()));
}

QNetworkReply* QWebDAV::deleteFile( QString name )
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // This is the URL of the webdav server + the file we want to get
    QUrl url(mHostname+name);

    // Finally send this to the WebDAV server
    QByteArray verb("DELETE");
    QNetworkReply *reply = sendWebdavRequest(url,DAVDELETE,verb);
    return reply;
}

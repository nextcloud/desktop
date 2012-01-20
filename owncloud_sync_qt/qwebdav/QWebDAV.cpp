/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync_qt.
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

#include "SyncGlobal.h"
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

// Qt File I/O related
#include <QFile>
#include <QFileInfo>

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
                                          QString extra, QString extra2)
{
    // Prepare the network request and headers
    QNetworkRequest request;
    QNetworkReply *reply;
    request.setUrl(url);
    request.setRawHeader(QByteArray("Host"),url.host().toUtf8());

    // First, find out what type we want
    if( type == DAVLIST ) {
        // A PROPFIND can include 0, 1 or infinity
        QString depthString = extra;
        request.setRawHeader(QByteArray("Depth"),
                             QByteArray(depthString.toAscii()));
        request.setAttribute(QNetworkRequest::User, QVariant("list"));
        request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+
                                                        ATTDATA)
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
        if ( mRequestFile.value(mRequestNumber) ) {
            request.setAttribute(QNetworkRequest::Attribute(
                                     QNetworkRequest::User+ATTFILE)
                                 ,QVariant(mRequestNumber));
            request.setAttribute(QNetworkRequest::Attribute(
                                     QNetworkRequest::User+ATTPREFIX)
                                 ,QVariant(extra.toAscii()));
            if( extra2 != 0 ) {
                // We were given a lock token.
                request.setRawHeader(QByteArray("If"),
                                     QByteArray(extra2.toAscii()));
            }
        } else {
            request.setAttribute(QNetworkRequest::Attribute(
                                     QNetworkRequest::User+ATTDATA)
                             ,QVariant(mRequestNumber));
        }
        reply = QNetworkAccessManager::put(request,data);
    } else if ( type == DAVMKCOL ) {
        request.setAttribute(QNetworkRequest::User, QVariant("mkcol"));
        reply = sendCustomRequest(request,verb,0);
    } else if ( type == DAVDELETE ) {
        request.setAttribute(QNetworkRequest::User, QVariant("delete"));
        reply = sendCustomRequest(request, verb,0);
    } else if ( type == DAVMOVE ) {
        request.setAttribute(QNetworkRequest::User, QVariant("move"));
        request.setRawHeader(QByteArray("Destination"),
                             QByteArray(extra.toAscii()));
        request.setRawHeader(QByteArray("Overwrite"),
                             QByteArray("T"));
        if( extra2 != 0 ) {
            // We were given (a) lock token(s).
            request.setRawHeader(QByteArray("If"),
                                 QByteArray(extra2.toAscii()));
            request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+
                                                            ATTLOCKTYPE)
                                 ,QVariant(extra.replace(mHostname,"").toAscii()));
        }
        reply = sendCustomRequest(request, verb,0);
    } else if ( type == DAVLOCK) {
        request.setAttribute(QNetworkRequest::User,
                             QVariant("lock"));
        // We don't bother setting a timeout, apparently the system defaults
        // to 5 minutes anyway.
        request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+
                                                        ATTDATA)
                             ,QVariant(mRequestNumber));
        request.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User+
                                                        ATTLOCKTYPE)
                             ,QVariant(extra));
        reply = sendCustomRequest(request,verb,data);
    } else if ( type == DAVUNLOCK) {
        QString token = "<"+extra+">";
        request.setAttribute(QNetworkRequest::User,
                             QVariant("unlock"));
        request.setRawHeader(QByteArray("Lock-Token"),
                             QByteArray(token.toAscii()));
        reply = sendCustomRequest(request,verb,0);
    } else {
        syncDebug() << "Error! DAV Request of type " << type << " is not known!";
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
    *query += "<D:prop xmlns:D=\"DAV:\">";
    *query += "<D:getlastmodified/>";
        *query += "<D:getlastmodified/>";
        *query += "<D:getcontentlength/>";
//        *query += "<D:resourcetype/>";
//        *query += "<D:quota-used-bytes/>";
//        *query += "<D:quota-available-bytes/>";
//            *query += "<D:getetag/>";
            *query += "<D:getcontenttype/>";
            *query += "<D:lockdiscovery/>";
        *query += "</D:prop>";
    *query += "</D:propfind>";
    QBuffer *data = new QBuffer(query);
    QByteArray verb("PROPFIND");
    mRequestQueries[mRequestNumber] = query;
    mRequestData[mRequestNumber] = data;
    // Finally send this to the WebDAV server
    return sendWebdavRequest(url,DAVLIST,verb,data,QString("%1").arg(depth));
}

void QWebDAV::slotReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    slotFinished(reply);
}

void QWebDAV::slotFinished(QNetworkReply *reply)
{
    bool keepReply = false;
    if ( reply->error() != 0 ) {
        syncDebug() << "WebDAV request returned error: " << reply->error()
                    << " On URL: " << reply->url().toString();
        if(reply->error()*0 == 299*0 )
            syncDebug() << reply->readAll();
    }

    // Good, but what is it responding to? Find out:
    if ( reply->request().attribute(
                QNetworkRequest::User).toString().contains("list") ) {
        //syncDebug() << "Oh a listing! How fun!!";
        processDirList(reply->readAll(),reply->url().path());
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("get") ) {
        //syncDebug() << "Oh a GET! How fun!!";
        //syncDebug() << "Get Data: "<< reply->readAll();
        processFile(reply);
        keepReply = true;
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("put")) {
        //syncDebug() << "Oh a PUT! How fun!!" <<
        //            reply->request().url().path().replace(mPathFilter,"");
        processPutFinished(reply);
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("mkcol")) {
        emit directoryCreated(reply->request().url().path().replace(
                                  QRegExp("^"+mPathFilter),""));
        //
        // Do nothing for now
    } else if( reply->request().attribute(
                   QNetworkRequest::User).toString().contains("move")) {
        // Check if we need to remove any locks, and for which file(s)?
        QString filename = reply->request().attribute(
                    QNetworkRequest::Attribute(
                    QNetworkRequest::User+ATTLOCKTYPE)).toString();
        if(filename != "" &&mTransferLockRequests.contains(filename)) {
            TransferLockRequest *request = &(mTransferLockRequests[filename]);
            unlock(request->fileNameTemp,request->tokenTemp);
            unlock(request->fileName,request->token);
        }

    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("delete")) {
        // Ok, that's great!
        // Do nothing
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("unlock")) {
        //syncDebug() << "Unlock reply: " << reply->readAll();
    } else if ( reply->request().attribute(
                    QNetworkRequest::User).toString().contains("lock")) {
        processLockRequest(reply->readAll(),reply->request().url().path()
                           .replace(QRegExp("^"+mPathFilter),""),
                           reply->request().attribute(
                               QNetworkRequest::Attribute(
                               QNetworkRequest::User+ATTLOCKTYPE)).toString());
    } else {
        syncDebug() << "Who knows what the server is trying to tell us. " +
                    reply->request().attribute(
                                       QNetworkRequest::User).toString();
    }
    // Now check if additional data needs to be deleted!
    qint64 value = reply->request().attribute(
                QNetworkRequest::Attribute(
                    QNetworkRequest::User+ATTDATA)).toLongLong();
    //syncDebug() << "Request number: " << value;
    if(value > 0 ) {
        delete mRequestData.value(value);
        delete mRequestQueries.value(value);
        mRequestData.remove(value);
        mRequestQueries.remove(value);
    }

    value = reply->request().attribute(
                QNetworkRequest::Attribute(
                    QNetworkRequest::User+ATTFILE)).toLongLong();
    if(value > 0) {
        delete mRequestFile.value(value);
        mRequestFile.remove(value);
    }

    if(!keepReply) {
        reply->deleteLater();
    }
}

void QWebDAV::processPutFinished(QNetworkReply *reply)
{
    // Check if a prefix exists that must be removed now that it finished
    QString prefix = reply->request().attribute(
                QNetworkRequest::Attribute(
                    QNetworkRequest::User+ATTPREFIX)).toString();
    if( prefix != "" ) {
        QString tokens = "";
        QString fileNameTemp = reply->request().url().toString().replace(mHostname,"/files/webdav.php/");
        QString to = reply->request().url().toString().replace(prefix,"");
        QString fileName = to;
        fileName.replace(mHostname,"");
        if(mTransferLockRequests.contains(fileName)) {
            TransferLockRequest *request = &(mTransferLockRequests[fileName]);
            tokens = "<"+fileNameTemp+ "> (<" + request->tokenTemp + ">)"
                    +"</files/webdav.php/" +fileName +"> (<"+request->token+">)";
        }
        QByteArray verb("MOVE");
        sendWebdavRequest(reply->request().url(),DAVMOVE,verb,0,
                          to,tokens);
    }
    emit uploadComplete(
                reply->request().url().path().replace(
                    QRegExp("^"+mPathFilter),"").replace(prefix,""));
}

void QWebDAV::slotAuthenticationRequired(QNetworkReply *reply,
                                         QAuthenticator *auth)
{
    //syncDebug() << "Authenticating: ";
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
    syncDebug() << "\n\n\n" << xml;
    QList<QWebDAV::FileInfo> list;
    QDomDocument domDocument;
    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!domDocument.setContent(xml, true, &errorStr, &errorLine,
                                &errorColumn)) {
        syncDebug() << "Error at line " << errorLine << " column " << errorColumn;
        syncDebug() << errorStr;
        emit directoryListingError(url);
        return;
    }

    QDomElement root = domDocument.documentElement();
    if( root.tagName() != "multistatus" ) {
        syncDebug() << "Badly formatted XML!" << xml;
        emit directoryListingError(url);
        return;
    }

    QString name;
    QString size;
    QString last;
    QString type;
    QString available;
    bool locked;
    QDomElement response = root.firstChildElement("response");
    while (!response.isNull()) {
        // Parse first response
        QDomElement child = response.firstChildElement();
        while (!child.isNull()) {
            //syncDebug() << "ChildName: " << child.tagName();
            if ( child.tagName() == "href" ) {
                name = child.text();
            } else if ( child.tagName() == "propstat") {
                QDomElement prop = child.firstChildElement("prop")
                        .firstChildElement();
                while(!prop.isNull()) {
                    //syncDebug() << "PropName: " << prop.tagName();
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
                    } else if ( prop.tagName() == "lockdiscovery") {
                        if(prop.text() == "" ) { // Not locked
                            locked = false;
                        } else { // Locked
                            QDomElement lock = prop.firstChildElement("activelock");
                            while(!lock.isNull()) {
                                if( prop.tagName() == "lockscope" &&
                                        prop.text() == "exclusive" ) {
                                    locked = true;
                                }
                                lock = lock.nextSiblingElement();
                            }
                        }
                    }

                    prop = prop.nextSiblingElement();
                }
            }
            child = child.nextSiblingElement();
        }
//        syncDebug() << "Name: " << name << "\nSize: " << size << "\nLastModified: "
//                 << last << "\nSizeAvailable: " << available << "\nType: "
//                 << type << "\n";
        // Filter out the requested directory from this list
        //syncDebug() << "Type: " << type << "Name: " << name << " URL: " << url;
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
    //syncDebug() << "GET REPLY: " << reply->readAll();
    return reply;
}

QNetworkReply* QWebDAV::put(QString fileName, QByteArray data,
                            QString put_prefix)
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // A put request is done in three steps:
    // 1) First lock the resource
    // 2) Put file
    // 3) Unlock resource
}

QNetworkReply* QWebDAV::put_locked(QString fileName, QByteArray data,
                                   QString put_prefix)
{

    // This is the Url of the webdav server + the file we want to get
    QUrl url(mHostname+fileName);

    // First lock the resource

    // Encapsulate data in an QIODevice
    mRequestNumber++;
    QByteArray *safeData = new QByteArray(data);
    QBuffer *buffer = new QBuffer(safeData);
    mRequestQueries[mRequestNumber] = safeData;
    mRequestData[mRequestNumber] = buffer;

    // Finally send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(url,DAVPUT,0,buffer);
    //syncDebug() << "PUT REPLY: " << reply->readAll();
    return reply;
}

QNetworkReply* QWebDAV::put(QString fileName, QString absoluteFileName,
                            QString put_prefix)
{
    // Make sure the user has already initialized this instance!
    if (!mInitialized)
        return 0;

    // A put request is done in three steps:
    // 1) First lock the resource
    // 2) Put file
    // 3) Unlock resource
    QString tempFileName = "";
    if ( put_prefix != "" ) {
        QFileInfo info(fileName);
        tempFileName = info.absolutePath()+"/"+put_prefix+
                   info.fileName();
    }
    mTransferLockRequests[fileName] = TransferLockRequest(
                true,fileName,tempFileName,absoluteFileName,put_prefix,
                new QWebDAVTransferRequestReply());
    lock(fileName,fileName);
    if ( put_prefix != "" ) {
        QFileInfo info(fileName);
        lock(info.absolutePath()+"/"+put_prefix+
                   info.fileName(),fileName);
    }
    return mTransferLockRequests[fileName].reply;
}

QNetworkReply* QWebDAV::put_locked(QString fileName, QString absoluteFileName,
                            QString put_prefix)
{
    // This is the Url of the webdav server + the file we want to put
    QUrl url;
    if ( put_prefix == "" ) {
        url.setUrl(mHostname+fileName);
    } else {
        QFileInfo info(fileName);
        url.setUrl(mHostname+info.absolutePath()+"/"+put_prefix+
                   info.fileName());
    }

    // Encapsulate data in an QIODevice
    mRequestNumber++;
    QFile *file = new QFile(absoluteFileName);
    if (!file->open(QIODevice::ReadOnly)) {
        syncDebug() << "File read error " + absoluteFileName +" Code: "
                    << file->error();
        return 0;
    }
    mRequestFile[mRequestNumber] = file;

    // Prepare the token
    TransferLockRequest *request = &(mTransferLockRequests[fileName]);
    QString tokens = "(<" + request->token + ">)"
            +"(<"+request->tokenTemp+">)";

    // Finally send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(url,DAVPUT,0,file,put_prefix,tokens);
    //syncDebug() << "PUT REPLY: " << reply->readAll();
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
    //syncDebug() << "MKCOL REPLY: " << reply->readAll();
    return reply;
}

void QWebDAV::slotReadyRead()
{
    //syncDebug() << "Data ready to be read!";
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

    //syncDebug() << "File Ready: " << fileName;
    emit fileReady(reply,fileName);
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

QNetworkReply *QWebDAV::lock( QString url, QString type )
{
    if( !mInitialized )
        return 0;

    // Prepare the query.
    mRequestNumber++;
    QByteArray *query = new QByteArray();
    *query += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
    *query += "<D:lockinfo xmlns:D=\"DAV:\">";
    *query += "<D:lockscope><D:exclusive/></D:lockscope>";
    *query += "<D:locktype><D:write/></D:locktype>";
    *query += "<D:owner>";
    *query += "<D:href>"+mUsername+"</D:href> ";
    *query += "</D:owner>";
    *query += "</D:lockinfo>";
    QBuffer *data = new QBuffer(query);
    mRequestQueries[mRequestNumber] = query;
    mRequestData[mRequestNumber] = data;
    QByteArray verb("LOCK");
    // Now send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(QUrl(mHostname+url),
                                             DAVLOCK,verb,data,type);
    return reply;
}

void QWebDAV::processLockRequest(QByteArray xml, QString url, QString extra)
{
    QDomDocument domDocument;
    QString errorStr;
    int errorLine;
    int errorColumn;
    if(!domDocument.setContent(xml,true,&errorStr,&errorLine,
            &errorColumn) ) {
        syncDebug() << "Error in lock at line " << errorLine << " column "
                       << errorColumn;
        syncDebug() << errorStr;
        return;
    }

    QDomElement root = domDocument.documentElement();
    if( root.tagName() != "prop" ) {
        // Check to see if it is reporting an error
        if( root.tagName() != "error") {
            syncDebug() << "Badly formatted XML! " << xml;
            return;
        } else { // Might already be locked
            QDomElement exception = root.firstChildElement("exception");
            if(!exception.isNull()&&exception.text()
                    =="Sabre_DAV_Exception_ConflictingLock") {
                syncDebug() << "Resource already locked!";
                if(extra != "") {
                    emit errorFileLocked(mTransferLockRequests[extra].fileName);
                }
            }
        }
    }

    QDomElement lockDiscovery = root.firstChildElement("lockdiscovery");
    if(!lockDiscovery.isNull() ) {
        QDomElement activeLock = lockDiscovery.firstChildElement("activelock");
        if(!activeLock.isNull()) {
            QDomElement locktoken = activeLock.firstChildElement("locktoken");
            if(!locktoken.isNull()) {
                QDomElement href = locktoken.firstChildElement("href");
                if(!href.isNull()) {
                    if(extra != "") {
                        TransferLockRequest *request = &(mTransferLockRequests[extra]);
                        if(url == request->fileName  ) { // This is the lock on
                            // the permanent file
                            request->token = href.text();
                        } else { // This is the lock on temporary file
                            request->tokenTemp = href.text();
                        }
                        if(request->put &&
                                request->token != ""
                                && (request->tokenTemp != ""
                                    || request->fileNameTemp == "")) {
                            request->reply->setReply(put_locked(request->fileName,
                                                    request->absoluteFileName,
                                                    request->put_prefix));
                        } else { // Get request

                        }
                    } else {
                        mLockTokens[url] = href.text();
                    }
                    syncDebug() << "Lock url: " <<
                                   url << "\tToken: " << href.text();
                }
            }
        }
    }
}

QNetworkReply *QWebDAV::unlock( QString url )
{
    if( !mInitialized )
        return 0;
    syncDebug() << "Will unlock: " << url << "\tToken: " << mLockTokens[url];
    return unlock(url,mLockTokens[url]);
}

QNetworkReply *QWebDAV::unlock( QString url, QString token )
{
    if( !mInitialized )
        return 0;

    QByteArray verb("UNLOCK");
    // Now send this to the WebDAV server
    QNetworkReply *reply = sendWebdavRequest(QUrl(mHostname+url)
                                             ,DAVUNLOCK,verb,0,token);
    return reply;
}

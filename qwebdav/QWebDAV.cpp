///////////////////////////////////////////////////////////////////////////////
//  File:       QWebDAV.cpp
//  Author(s):  Juan Carlos Cornejo <jc2@paintblack.com>
//  Summary:
///////////////////////////////////////////////////////////////////////////////

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

QWebDAV::QWebDAV(QObject *parent) :
    QNetworkAccessManager(parent), mInitialized(false)
{
}

void QWebDAV::initialize(QString hostname, QString username, QString password,
                         QString pathFilter)
{
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
        request.setRawHeader(QByteArray("Content-Type"),
                             QByteArray("text/xml; charset=\"utf-8\""));
        request.setRawHeader(QByteArray("Content-Length"),QByteArray("99999"));

        reply = sendCustomRequest(request,verb,mData1);
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
        reply = QNetworkAccessManager::put(request,data);
    } else if ( type == DAVMKCOL ) {
        reply = sendCustomRequest(request,verb,0);
    } else if ( type == DAVDELETE ) {
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
    mQuery1 = new QByteArray();
    *mQuery1 += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>";
    *mQuery1 += "<D:propfind xmlns:D=\"DAV:\">";
    *mQuery1 += "<D:allprop/>";
    *mQuery1 += "</D:propfind>";
    if (mData1) {
        //delete mData1;
    }
    mData1 = new QBuffer(mQuery1);
    QByteArray verb("PROPFIND");

    // Finally send this to the WebDAV server
    return sendWebdavRequest(url,DAVLIST,verb,mData1,depth);
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
                    reply->request().url().path().replace(mPathFilter,""));

    }

}

void QWebDAV::slotAuthenticationRequired(QNetworkReply *reply,
                                         QAuthenticator *auth)
{
    //qDebug() << "Authenticating:";
    if(auth) {
        auth->setUser(mUsername);
        auth->setPassword(mPassword);
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
        return;
    }

    QDomElement root = domDocument.documentElement();
    if( root.tagName() != "multistatus" ) {
        qDebug() << "Badly formatted XML!" << xml;
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
    QByteArray *safeData = new QByteArray(data);
    QBuffer *buffer = new QBuffer(safeData);

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
    QNetworkReply *reply = sendWebdavRequest(url,DAVMKCOL,verb);
    return reply;
}

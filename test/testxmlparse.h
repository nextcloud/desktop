/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_TESTXMLPARSE_H
#define MIRALL_TESTXMLPARSE_H

#include <QtTest>

#include "networkjobs.h"

using namespace OCC;

class TestXmlParse : public QObject
{
    Q_OBJECT

private:
  bool _success;
  QStringList _subdirs;
  QStringList _items;

private slots:
    void initTestCase() {
      _success = false;
    }

    void cleanupTestCase() {
    }

    void slotDirectoryListingSubFolders(const QStringList& list)
    {
       qDebug() << "subfolders: " << list;
       _subdirs.append(list);
    }

    void slotDirectoryListingIterated(const QString& item, const QMap<QString,QString>& )
    {
      qDebug() << "     item: " << item;
      _items.append(item);
    }

    void slotFinishedSuccessfully()
    {
        _success = true;
    }

    void testParser1() {
        const QByteArray testXml = "<?xml version='1.0' encoding='utf-8'?>"
              "<d:multistatus xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" xmlns:oc=\"http://owncloud.org/ns\">"
              "<d:response>"
              "<d:href>/oc/remote.php/webdav/sharefolder/</d:href>"
              "<d:propstat>"
              "<d:prop>"
              "<oc:id>00004213ocobzus5kn6s</oc:id>"
              "<oc:permissions>RDNVCK</oc:permissions>"
              "<oc:size>121780</oc:size>"
              "<d:getetag>\"5527beb0400b0\"</d:getetag>"
              "<d:resourcetype>"
              "<d:collection/>"
              "</d:resourcetype>"
              "<d:getlastmodified>Fri, 06 Feb 2015 13:49:55 GMT</d:getlastmodified>"
              "</d:prop>"
              "<d:status>HTTP/1.1 200 OK</d:status>"
              "</d:propstat>"
              "<d:propstat>"
              "<d:prop>"
              "<d:getcontentlength/>"
              "<oc:downloadURL/>"
              "<oc:dDC/>"
              "</d:prop>"
              "<d:status>HTTP/1.1 404 Not Found</d:status>"
              "</d:propstat>"
              "</d:response>"
              "<d:response>"
              "<d:href>/oc/remote.php/webdav/sharefolder/quitte.pdf</d:href>"
              "<d:propstat>"
              "<d:prop>"
              "<oc:id>00004215ocobzus5kn6s</oc:id>"
              "<oc:permissions>RDNVW</oc:permissions>"
              "<d:getetag>\"2fa2f0d9ed49ea0c3e409d49e652dea0\"</d:getetag>"
              "<d:resourcetype/>"
              "<d:getlastmodified>Fri, 06 Feb 2015 13:49:55 GMT</d:getlastmodified>"
              "<d:getcontentlength>121780</d:getcontentlength>"
              "</d:prop>"
              "<d:status>HTTP/1.1 200 OK</d:status>"
              "</d:propstat>"
              "<d:propstat>"
              "<d:prop>"
              "<oc:downloadURL/>"
              "<oc:dDC/>"
              "</d:prop>"
              "<d:status>HTTP/1.1 404 Not Found</d:status>"
              "</d:propstat>"
              "</d:response>"
              "</d:multistatus>";


        LsColXMLParser parser;

        connect( &parser, SIGNAL(directoryListingSubfolders(const QStringList&)),
                 this, SLOT(slotDirectoryListingSubFolders(const QStringList&)) );
        connect( &parser, SIGNAL(directoryListingIterated(const QString&, const QMap<QString,QString>&)),
                 this, SLOT(slotDirectoryListingIterated(const QString&, const QMap<QString,QString>&)) );
        connect( &parser, SIGNAL(finishedWithoutError()),
                 this, SLOT(slotFinishedSuccessfully()) );

        QHash <QString, qint64> sizes;
        parser.parse( testXml, &sizes );

        QVERIFY(_success);
        QVERIFY(sizes.size() == 0 ); // No quota info in the XML

        QVERIFY(_items.contains("/oc/remote.php/webdav/sharefolder/quitte.pdf"));
        QVERIFY(_items.contains("/oc/remote.php/webdav/sharefolder"));
        QVERIFY(_items.size() == 2 );

        QVERIFY(_subdirs.contains("/oc/remote.php/webdav/sharefolder/"));
        QVERIFY(_subdirs.size() == 1);
    }
};

#endif

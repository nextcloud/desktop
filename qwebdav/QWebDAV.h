#ifndef QWEBDAV_H
#define QWEBDAV_H

#include <QNetworkAccessManager>
#include <QSslError>
#include <QDebug>
#include <QNetworkReply>

class QNetworkReply;
class QBuffer;
class QUrl;


class QWebDAV : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit QWebDAV(QObject *parent = 0);
    void initialize(QString hostname, QString username, QString password,
                    QString pathFilter = "");

    enum DAVType {
        DAVLIST,
        DAVGET,
        DAVPUT,
        DAVMKCOL
    };

    struct FileInfo {
        QString fileName;
        QString lastModified;
        qlonglong size;
        qlonglong sizeAvailable;
        QString type;
        FileInfo(QString name, QString last, qlonglong fileSize,
                 qlonglong available, QString fileType ) {
            fileName = name;
            lastModified = last;
            size = fileSize;
            sizeAvailable = available;
            if( fileType == "" ) {
                type = "file";
            } else {
                type = fileType;
            }
        };
        QString toString()
        {
            QString available;
            if( type == "collection") {
                available = QString("\nAvailable Space: %1").arg(sizeAvailable);
            }
            QString output = QString("\nFile Name:       %1\nLast Modified:   %2"
                                     "\nFile Size:       %3\nType:            %4"
                                     )
                                     .arg(fileName)
                                     .arg(lastModified).arg(size).arg(type);
                    output += available;
            output.append("\n\n\n");
            return output;

        }
        void print() {
            qDebug() << toString().toAscii();
        }

        QString formatSize()
        {
            //if ( size > 1099511627776 ) // 1TB
            return QString();
        }
    };

    // DAV Public Functions
    void dirList(QString dir = "/");
    QNetworkReply* list(QString dir, int depth = 1);
    QNetworkReply* get(QString fileName );
    QNetworkReply* put(QString fileName , QByteArray data);
    QNetworkReply* mkdir(QString dirName );
    QNetworkReply* sendWebdavRequest( QUrl url, DAVType type,
                                      QByteArray verb = 0,QIODevice *data = 0,
                                      int depth = 1);

private:
    QString mHostname;
    QString mUsername;
    QString mPassword;
    QString mPathFilter;
    bool mInitialized;
    QBuffer *mData1;
    QByteArray *mQuery1;

    void processDirList(QByteArray xml, QString url);
    void processFile(QNetworkReply* reply);
    void processLocalDirectory(QString dirPath);
    void connectReplyFinished(QNetworkReply *reply);

signals:
    void directoryListingReady(QList<QWebDAV::FileInfo>);
    void fileReady(QByteArray data, QString fileName);
    void uploadComplete(QString name);

public slots:
    void slotFinished ( QNetworkReply* );
    void slotReplyFinished();
    void slotAuthenticationRequired(QNetworkReply*, QAuthenticator*);
    void slotReadyRead();
    void slotSslErrors(QList<QSslError> errorList);
    void slotError(QNetworkReply::NetworkError error);

};

#endif // QWEBDAV_H

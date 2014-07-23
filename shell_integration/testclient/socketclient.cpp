
#include "socketclient.h"

SocketClient::SocketClient(QObject *parent) :
    QObject(parent)
  , sock(new QLocalSocket(this))
{
    QString sockAddr;
#ifdef Q_OS_UNIX
    sockAddr = QDir::homePath() + QLatin1String("/.local/share/data/ownCloud/");
#else
    sockAddr = QLatin1String("\\\\.\\pipe\\ownCloud");
#endif
    sock->connectToServer(sockAddr);
    sock->open(QIODevice::ReadWrite);
    connect(sock, SIGNAL(readyRead()), SLOT(writeData()));
}

void SocketClient::writeData()
{
    qDebug() << sock->readAll();
}

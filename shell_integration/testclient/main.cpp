#include <QApplication>
#include <QLocalSocket>
#include <QDir>

#include "window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QLocalSocket sock;
    QString sockAddr;
#ifdef Q_OS_UNIX
    sockAddr = QDir::homePath() + QLatin1String("/.local/share/data/ownCloud/socket");
#else
    sockAddr = QLatin1String("\\\\.\\pipe\\ownCloud");
#endif
    Window win(&sock);
    QObject::connect(&sock, SIGNAL(readyRead()), &win, SLOT(receive()));
    QObject::connect(&sock, SIGNAL(error(QLocalSocket::LocalSocketError)),
                     &win, SLOT(receiveError(QLocalSocket::LocalSocketError)));

    win.show();
    sock.connectToServer(sockAddr, QIODevice::ReadWrite);
    qDebug() << "Connecting to" << sockAddr;

    return app.exec();
}

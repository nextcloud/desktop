#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <QObject>

class QLocalSocket;

class SocketClient : public QObject
{
    Q_OBJECT
public:
    explicit SocketClient(QObject *parent = 0);

public slots:
    void writeData();

private:
    QLocalSocket *sock;
};

#endif // SOCKETCLIENT_H

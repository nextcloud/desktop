#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QLocalSocket>

namespace Ui {
class Window;
}

class Window : public QWidget
{
    Q_OBJECT

public:
    explicit Window(QIODevice *dev, QWidget *parent = 0);
    ~Window();

public slots:
    void receive();
    void receiveError(QLocalSocket::LocalSocketError);

private slots:
    void handleReturn();

private:
    void addDefaultItems();
    Ui::Window *ui;
    QIODevice *device;
};

#endif // WINDOW_H

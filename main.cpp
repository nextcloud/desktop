#include <QtGui/QApplication>
#include "SyncWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SyncWindow w;
    w.show();

    return a.exec();
}

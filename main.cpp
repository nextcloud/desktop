#include <QtGui/QApplication>
#include "SyncWindow.h"
#include <QSharedMemory>

int main(int argc, char *argv[])
{
    // Technique for having only one application running at the same time per
    // computer.
    QSharedMemory sharedMemory;
    sharedMemory.setKey("8SyGEyz2kcI0UNrqxc7wcuKLjD1pSL8GezlBM1W0QckgpjMon0");
    if(sharedMemory.attach()) {
            return -1;
    }

    if (!sharedMemory.create(1)) {
            return -1; // Exit already a process running
    }

    // Great. No other instance is running, so we can run the program.
    QApplication a(argc, argv);
    SyncWindow w;
    //w.show();

    return a.exec();
}

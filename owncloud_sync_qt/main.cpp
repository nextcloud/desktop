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
#include <QtGui/QApplication>
#include "SyncWindow.h"
#include <QSharedMemory>
#include <QMessageBox>

int quitOtherInstanceRunning()
{
    QMessageBox box;
    box.setText(QMessageBox::tr("Either another instance is already running or "
                                "the last instance crashed. You may try running"
                                " this again to clear it."));
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();
    return -1;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Technique for having only one application running at the same time per
    // computer.
    QSharedMemory sharedMemory;
    sharedMemory.setKey("8SyGEyz2kcI0UNrqxc7wcuKLjD1pSL8GezlBM1W0QckgpjMon0");
    if(sharedMemory.attach()) {
        return quitOtherInstanceRunning();
    }

    if (!sharedMemory.create(1)) {
        return quitOtherInstanceRunning();
    }

    // Great. No other instance is running, so we can run the program.
    SyncWindow w;
    //w.show();

    return a.exec();
}

/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync.
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

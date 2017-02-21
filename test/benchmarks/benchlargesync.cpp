/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

int numDirs = 0;
int numFiles = 0;

template<int filesPerDir, int dirPerDir, int maxDepth>
void addBunchOfFiles(int depth, const QString &path, FileModifier &fi) {
    for (int fileNum = 1; fileNum <= filesPerDir; ++fileNum) {
        QString name = QStringLiteral("file") + QString::number(fileNum);
        fi.insert(path.isEmpty() ? name : path + "/" + name);
        numFiles++;
    }
    if (depth >= maxDepth)
        return;
    for (char dirNum = 1; dirNum <= dirPerDir; ++dirNum) {
        QString name = QStringLiteral("dir") + QString::number(dirNum);
        QString subPath = path.isEmpty() ? name : path + "/" + name;
        fi.mkdir(subPath);
        numDirs++;
        addBunchOfFiles<filesPerDir, dirPerDir, maxDepth>(depth + 1, subPath, fi);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    FakeFolder fakeFolder{FileInfo{}};
    addBunchOfFiles<10, 8, 4>(0, "", fakeFolder.localModifier());

    qDebug() << "NUMFILES" << numFiles;
    qDebug() << "NUMDIRS" << numDirs;
    return fakeFolder.syncOnce() ? 0 : -1;
}

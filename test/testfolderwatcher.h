
#ifndef MIRALL_TEST_FOLDERWATCHER_H
#define MIRALL_TEST_FOLDERWATCHER_H

#include <QtTest/QtTest>

class Mirall::FolderWatcher;

class TestFolderWatcher : public QObject
{
    Q_OBJECT
public:

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFilesAdded();

private:
    Mirall::FolderWatcher *_watcher;
};


#endif



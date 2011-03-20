#ifndef MIRALL_FOLDER_H
#define MIRALL_FOLDER_H

#include <QObject>
#include <QString>

class QAction;

namespace Mirall {

class FolderWatcher;

class Folder : public QObject
{
    Q_OBJECT

public:
    Folder(const QString &path, QObject *parent = 0L);
    virtual ~Folder();

    QString path() const;

    QAction *action() const;

    /**
     * starts a sync operation
     * requests are serialized
     */
    virtual void startSync() = 0;

signals:
    void syncStarted();
    void syncFinished();

protected:

private:
    QString _path;
    FolderWatcher *_watcher;
    QAction *_action;
private slots:
    void slotChanged(const QString &path);
    void slotOpenFolder();
};

}

#endif

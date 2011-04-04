/**
Based on example by:
Copyright (c) Ashish Shukla

This is licensed under GNU GPL v2 or later.
For license text, Refer to the the file COPYING or visit
http://www.gnu.org/licenses/gpl.txt .
*/

#ifndef MIRALL_INOTIFY_H
#define MIRALL_INOTIFY_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QThread>

namespace Mirall
{

class INotify : public QObject
{
    Q_OBJECT

public:

    INotify(int mask);
    ~INotify();

    static void initialize();
    static void cleanup();

    void addPath(const QString &name);
    void removePath(const QString &name);

    QStringList directories() const;
signals:

    void notifyEvent(int mask, int cookie, const QString &name);

private:
    class INotifyThread : public QThread
    {
        int _fd;
        QHash<int, INotify*> _map;
    public:
        INotifyThread(int fd);
        ~INotifyThread();
        void registerForNotification(INotify*, int);
        void unregisterForNotification(INotify*);
        // fireEvent happens from the inotify thread
        // but addPath comes from the main thread
        static QMutex s_mutex;
    protected:
        void run();
    private:
        size_t _buffer_size;
        char *_buffer;
    };

    //INotify(int wd);
    void fireEvent(int mask, int cookie, int wd, char *name);
    static int s_fd;
    static INotifyThread* s_thread;

    // the mask is shared for all paths
    int _mask;
    QMap<QString, int> _wds;
};

}

#endif

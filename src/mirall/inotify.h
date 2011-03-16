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
#include <QHash>
#include <QString>
#include <QThread>

namespace Mirall
{

class INotify : public QObject
{
    Q_OBJECT

public:

    INotify(const QString &name, int mask);
    ~INotify();

    static void initialize();
    static void cleanup();

signals:

    void notifyEvent(int mask, const QString &name);

private:
    class INotifyThread : public QThread
    {
        int _fd;
        QHash<int, INotify*> _map;
    public:
        INotifyThread(int fd);
        void registerForNotification(INotify*);
        void unregisterForNotification(INotify*);
    protected:
        void run();
    };

    INotify(int wd);
    void fireEvent(int mask, char *name);
    static int s_fd;
    static INotifyThread* s_thread;
    int _wd;
};

}

#endif

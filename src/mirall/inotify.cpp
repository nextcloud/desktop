/**
Based on example by:
Copyright (c) Ashish Shukla

This is licensed under GNU GPL v2 or later.
For license text, Refer to the the file COPYING or visit
http://www.gnu.org/licenses/gpl.txt .
*/

#include <sys/inotify.h>
#include <cerrno>
#include <unistd.h>
#include <QDebug>
#include <QStringList>

#include "inotify.h"


// Buffer Size for read() buffer
#define DEFAULT_READ_BUFFERSIZE 2048

namespace Mirall {

// Allocate space for static members of class.
int INotify::s_fd;
INotify::INotifyThread* INotify::s_thread;

//INotify::INotify(int wd) : _wd(wd)
//{
//}

INotify::INotify(int mask) : _mask(mask)
{
}

INotify::~INotify()
{
    // Unregister from iNotifier thread.
    s_thread->unregisterForNotification(this);

    // Remove all inotify watchs.
    QString key;
    foreach (key, _wds.keys())
        inotify_rm_watch(s_fd, _wds.value(key));
}

void INotify::addPath(const QString &path)
{
    // Add an inotify watch.
    path.toAscii().constData();

    int wd = inotify_add_watch(s_fd, path.toAscii().constData(), _mask);
    _wds[path] = wd;

    // Register for iNotifycation from iNotifier thread.
    s_thread->registerForNotification(this, wd);
}

void INotify::removePath(const QString &path)
{
    // Remove the inotify watch.
    inotify_rm_watch(s_fd, _wds[path]);
    _wds.remove(path);
}

QStringList INotify::directories() const
{
    return _wds.keys();
}

void
INotify::INotifyThread::unregisterForNotification(INotify* notifier)
{
    //_map.remove(notifier->_wd);
    QHash<int, INotify*>::iterator it;
    for (it = _map.begin(); it != _map.end(); ++it) {
        if (it.value() == notifier)
            _map.remove(it.key());
    }
}

void
INotify::INotifyThread::registerForNotification(INotify* notifier, int wd)
{
    _map[wd] = notifier;
}

void
INotify::fireEvent(int mask, int cookie, int wd, char* name)
{
    QString path;
    foreach (path, _wds.keys(wd))
        emit notifyEvent(mask, cookie, path + "/" + QString::fromUtf8(name));
}

void
INotify::initialize()
{
    s_fd = inotify_init();
    s_thread = new INotifyThread(s_fd);
    s_thread->start();
}

void
INotify::cleanup()
{
    close(s_fd);
    s_thread->terminate();
    s_thread->wait(3000);
    delete s_thread;
}

INotify::INotifyThread::INotifyThread(int fd) : _fd(fd)
{
    _buffer_size = DEFAULT_READ_BUFFERSIZE;
    _buffer = (char *) malloc(_buffer_size);
}

INotify::INotifyThread::~INotifyThread()
{
    free(_buffer);
}

// Thread routine
void
INotify::INotifyThread::run()
{
    int len;
    struct inotify_event* event;
    INotify* n = NULL;
    int i;
    int error;

    // main loop
    while (true) {
        do {
            len = read(_fd, _buffer, _buffer_size);
            error = errno;
            /**
             * From inotify documentation:
             *
             * The behavior when the buffer given to read(2) is too
             * small to return information about the next event
             * depends on the kernel version: in kernels  before 2.6.21,
             * read(2) returns 0; since kernel 2.6.21, read(2) fails with
             * the error EINVAL.
             */
            if (len < 0 && error == EINVAL)
            {
                // double the buffer size
                qWarning() << "buffer size too small";
                _buffer_size *= 2;
                _buffer = (char *) realloc(_buffer, _buffer_size);
                /* and try again ... */
                continue;
            }
        } while (false);

        /* TODO handle len == 0 */

        // reset counter
        i = 0;
        // while there are enough events in the buffer
        while(i + sizeof(struct inotify_event) < len) {
            // cast an inotify_event
            event = (struct inotify_event*)&_buffer[i];
            // with the help of watch descriptor, retrieve, corresponding INotify
            if (event == NULL) {
                qDebug() << "NULL event";
                continue;
            }
            else if (event && (IN_IGNORED & event->mask)) {
                qDebug() << "IGNORE event";
                continue;
            }

            if (event && (IN_Q_OVERFLOW & event->mask))
                qDebug() << "OVERFLOW";

            n = _map[event->wd];

            // fire event
            if (event->len > 0)
                n->fireEvent(event->mask, event->cookie, event->wd, event->name);
            // increment counter
            i += sizeof(struct inotify_event) + event->len;
        }
    }
}

} // ns mirall

#include "inotify.moc"

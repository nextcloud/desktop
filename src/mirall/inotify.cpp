/**
Based on example by:
Copyright (c) Ashish Shukla

This is licensed under GNU GPL v2 or later.
For license text, Refer to the the file COPYING or visit
http://www.gnu.org/licenses/gpl.txt .
*/

#include <sys/inotify.h>
#include <unistd.h>

#include "inotify.h"

// Buffer Size for read() buffer
#define BUFFERSIZE 512

namespace Mirall {

// Allocate space for static members of class.
int INotify::s_fd;
INotify::INotifyThread* INotify::s_thread;

INotify::INotify(int wd) : _wd(wd)
{
}

INotify::INotify(const QString &path, int mask)
{
    // Add an inotify watch.
    _wd = inotify_add_watch(s_fd, path.toUtf8().data(), mask);

    // Register for iNotifycation from iNotifier thread.
    s_thread->registerForNotification(this);
}

INotify::~INotify()
{
    // Unregister from iNotifier thread.
    s_thread->unregisterForNotification(this);

    // Remove the inotify watch.
    inotify_rm_watch(s_fd, _wd);
}

void
INotify::INotifyThread::unregisterForNotification(INotify* notifier)
{
    _map.remove(notifier->_wd);
}

void
INotify::INotifyThread::registerForNotification(INotify* notifier)
{
    _map[notifier->_wd] = notifier;
}

void
INotify::fireEvent(int mask, char* name)
{
    emit notifyEvent(mask, QString::fromUtf8(name));
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
}


// Thread routine
void
INotify::INotifyThread::run()
{
    int len;
    struct inotify_event* event;
    char buffer[BUFFERSIZE];
    INotify* n = NULL;
    int i;

    // read the inotify file descriptor.
    while((len = read(_fd, buffer, BUFFERSIZE)) > 0)
    {
        // reset counter
        i = 0;
        // while there are enough events in the buffer
        while(i + sizeof(struct inotify_event) < len)
        {
            // cast an inotify_event
            event = (struct inotify_event*)&buffer[i];
            // with the help of watch descriptor, retrieve, corresponding INotify
            n = _map[event->wd];
            // fire event
            n->fireEvent(event->mask, event->name);
            // increment counter
            i += sizeof(struct inotify_event) + event->len;
        }
    }
}

} // ns mirall

#include "inotify.moc"

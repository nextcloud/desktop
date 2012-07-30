/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: http://www.qt-project.org/
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**************************************************************************/

#include "qtlockedfile.h"

#include <qt_windows.h>
#include <QFileInfo>

namespace SharedTools {

#define SEMAPHORE_PREFIX "QtLockedFile semaphore "
#define MUTEX_PREFIX "QtLockedFile mutex "
#define SEMAPHORE_MAX 100

static QString errorCodeToString(DWORD errorCode)
{
    QString result;
    char *data = 0;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, errorCode, 0,
                    (char*)&data, 0, 0);
    result = QString::fromLocal8Bit(data);
    if (data != 0)
        LocalFree(data);

    if (result.endsWith('\n'))
        result.truncate(result.length() - 1);

    return result;
}

bool QtLockedFile::lock(LockMode mode, bool block)
{
    if (!isOpen()) {
        qWarning("QtLockedFile::lock(): file is not opened");
        return false;
    }

    if (mode == m_lock_mode)
        return true;

    if (m_lock_mode != 0)
        unlock();

    if (m_semaphore_hnd == 0) {
        QFileInfo fi(*this);
        QString sem_name = QString::fromLatin1(SEMAPHORE_PREFIX)
                           + fi.absoluteFilePath().toLower();

        m_semaphore_hnd = CreateSemaphoreW(0, SEMAPHORE_MAX, SEMAPHORE_MAX,
                                           (TCHAR*)sem_name.utf16());

        if (m_semaphore_hnd == 0) {
            qWarning("QtLockedFile::lock(): CreateSemaphore: %s",
                     errorCodeToString(GetLastError()).toLatin1().constData());
            return false;
        }
    }

    bool gotMutex = false;
    int decrement;
    if (mode == ReadLock) {
        decrement = 1;
    } else {
        decrement = SEMAPHORE_MAX;
        if (m_mutex_hnd == 0) {
            QFileInfo fi(*this);
            QString mut_name = QString::fromLatin1(MUTEX_PREFIX)
                               + fi.absoluteFilePath().toLower();

            m_mutex_hnd = CreateMutexW(NULL, FALSE, (TCHAR*)mut_name.utf16());

            if (m_mutex_hnd == 0) {
                qWarning("QtLockedFile::lock(): CreateMutex: %s",
                         errorCodeToString(GetLastError()).toLatin1().constData());
                return false;
            }
        }
        DWORD res = WaitForSingleObject(m_mutex_hnd, block ? INFINITE : 0);
        if (res == WAIT_TIMEOUT)
            return false;
        if (res == WAIT_FAILED) {
            qWarning("QtLockedFile::lock(): WaitForSingleObject (mutex): %s",
                     errorCodeToString(GetLastError()).toLatin1().constData());
            return false;
        }
        gotMutex = true;
    }

    for (int i = 0; i < decrement; ++i) {
        DWORD res = WaitForSingleObject(m_semaphore_hnd, block ? INFINITE : 0);
        if (res == WAIT_TIMEOUT) {
            if (i) {
                // A failed nonblocking rw locking. Undo changes to semaphore.
                if (ReleaseSemaphore(m_semaphore_hnd, i, NULL) == 0) {
                    qWarning("QtLockedFile::unlock(): ReleaseSemaphore: %s",
                             errorCodeToString(GetLastError()).toLatin1().constData());
                    // Fall through
                }
            }
            if (gotMutex)
                ReleaseMutex(m_mutex_hnd);
            return false;
	}
        if (res != WAIT_OBJECT_0) {
            if (gotMutex)
                ReleaseMutex(m_mutex_hnd);
            qWarning("QtLockedFile::lock(): WaitForSingleObject (semaphore): %s",
                        errorCodeToString(GetLastError()).toLatin1().constData());
            return false;
        }
    }

    m_lock_mode = mode;
    if (gotMutex)
        ReleaseMutex(m_mutex_hnd);
    return true;
}

bool QtLockedFile::unlock()
{
    if (!isOpen()) {
        qWarning("QtLockedFile::unlock(): file is not opened");
        return false;
    }

    if (!isLocked())
        return true;

    int increment;
    if (m_lock_mode == ReadLock)
        increment = 1;
    else
        increment = SEMAPHORE_MAX;

    DWORD ret = ReleaseSemaphore(m_semaphore_hnd, increment, 0);
    if (ret == 0) {
        qWarning("QtLockedFile::unlock(): ReleaseSemaphore: %s",
                    errorCodeToString(GetLastError()).toLatin1().constData());
        return false;
    }

    m_lock_mode = QtLockedFile::NoLock;
    return true;
}

QtLockedFile::~QtLockedFile()
{
    if (isOpen())
        unlock();
    if (m_mutex_hnd != 0) {
        DWORD ret = CloseHandle(m_mutex_hnd);
        if (ret == 0) {
            qWarning("QtLockedFile::~QtLockedFile(): CloseHandle (mutex): %s",
                        errorCodeToString(GetLastError()).toLatin1().constData());
        }
        m_mutex_hnd = 0;
    }
    if (m_semaphore_hnd != 0) {
        DWORD ret = CloseHandle(m_semaphore_hnd);
        if (ret == 0) {
            qWarning("QtLockedFile::~QtLockedFile(): CloseHandle (semaphore): %s",
                        errorCodeToString(GetLastError()).toLatin1().constData());
        }
        m_semaphore_hnd = 0;
    }
}

} // namespace SharedTools

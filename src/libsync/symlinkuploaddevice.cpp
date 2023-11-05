#include "symlinkuploaddevice.h"

#include <QFileInfo>
#include <unistd.h>

namespace {
// Helper function to read raw symlink target;
// QFileInfo::readSymLink() only available in Qt 6.6 or newer
// and QFileInfo::symLinkTarget() will transform path to absolute path
// which might break relative symlinks for cross-device synchronization
QString readRawSymlink(const QString& path)
{
#if defined(Q_OS_UNIX) || defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    QByteArray buffer(255, '\0');
    ssize_t readLength{};

    auto tryReadlink = [&path, &buffer]() {
        return readlink(path.toUtf8().data(), buffer.data(), buffer.size());
    };
    while ((readLength = tryReadlink()) >= static_cast<ssize_t>(buffer.size())) {
        buffer.resize(buffer.size() + 100);
    }

    if (readLength > 0)
    {
        buffer[static_cast<unsigned int>(readLength)] = '\0';
        return QString(buffer);
    }
#else
    Q_UNUSED(path);
#endif
    return QString();
}
}

namespace OCC {
SymLinkUploadDevice::SymLinkUploadDevice(const QString &fileName, qint64 start, qint64 size, BandwidthManager *bwm)
    : UploadDevice(fileName, start, size, bwm)
{
}

bool SymLinkUploadDevice::open(QIODevice::OpenMode mode)
{
    if (mode & QIODevice::WriteOnly)
        return false;

    auto symlinkContent = readRawSymlink(_file.fileName());
    if (symlinkContent.isEmpty()) {
        setErrorString("Unable to read symlink '" + _file.fileName() + "'");
        return false;
    }

    _symlinkContent = symlinkContent.toUtf8();
    _size = qBound(0ll, _size, _symlinkContent.size() - _start);
    _read = 0;

    return QIODevice::open(mode);
}

qint64 SymLinkUploadDevice::readData(char *data, qint64 maxlen)
{
    if (_size - _read <= 0) {
        // at end
        if (_bandwidthManager) {
            _bandwidthManager->unregisterUploadDevice(this);
        }
        return -1;
    }
    maxlen = qMin(maxlen, _size - _read);
    if (maxlen <= 0) {
        return 0;
    }
    if (isChoked()) {
        return 0;
    }
    if (isBandwidthLimited()) {
        maxlen = qMin(maxlen, _bandwidthQuota);
        if (maxlen <= 0) { // no quota
            return 0;
        }
        _bandwidthQuota -= maxlen;
    }

    auto readStart = _start + _read;
    auto readEnd = _size;
    if (_size - _read < maxlen) {
        _read = _size;
    } else {
        _read += maxlen;
        readEnd = _start + _read;
    }
    Q_ASSERT(readEnd <= _symlinkContent.size());
    std::copy(_symlinkContent.begin() + readStart, _symlinkContent.begin() + readEnd, data);
    return readEnd - readStart;
}

bool SymLinkUploadDevice::seek(qint64 pos)
{
    if (!QIODevice::seek(pos)) {
        return false;
    }
    if (pos < 0 || pos > _size) {
        return false;
    }
    _read = pos;
    return true;
}
}

#include "symlinkuploaddevice.h"

#include "filesystem.h"

#include <QFileInfo>

namespace OCC {
SymLinkUploadDevice::SymLinkUploadDevice(const QString &fileName, qint64 start, qint64 size, BandwidthManager *bwm)
    : UploadDevice(fileName, start, size, bwm)
{
}

bool SymLinkUploadDevice::open(QIODevice::OpenMode mode)
{
    if (mode & QIODevice::WriteOnly)
        return false;

    _symlinkContent = FileSystem::readlink(_file.fileName());
    if (_symlinkContent.isEmpty()) {
        setErrorString("Unable to read symlink '" + _file.fileName() + "'");
        return false;
    }

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

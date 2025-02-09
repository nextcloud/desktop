
#include "kfcompat.h"

//-----------------------------------------------------------------------------

#include <QByteArray>
#include <QBuffer>

//-----------------------------------------------------------------------------

#ifdef HAVE_KARCHIVE

//-----------------------------------------------------------------------------

#include <KCompressionDevice>

//-----------------------------------------------------------------------------

QByteArray gZipData(const QByteArray& inputData)
{
    QBuffer gZipBuffer;
    auto gZipCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipCompressionDevice.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipCompressionDevice.write(inputData);
    gZipCompressionDevice.close();
    if (bytesWritten < 0) {
        return {};
    }

    if (!gZipBuffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto gZipped = gZipBuffer.readAll();
    gZipBuffer.close();
    return gZipped;
}

//-----------------------------------------------------------------------------

QByteArray unGzipData(const QByteArray& inputData)
{
    QBuffer gZipBuffer;
    if (!gZipBuffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipBuffer.write(inputData);
    gZipBuffer.close();
    if (bytesWritten < 0) {
        return {};
    }

    auto gZipUnCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipUnCompressionDevice.open(QIODevice::ReadOnly)) {
        return {};
    }

    auto unGzipped = gZipUnCompressionDevice.readAll();
    gZipUnCompressionDevice.close();
    return unGzipped;
}


//-----------------------------------------------------------------------------
#else // HAVE_KARCHIVE
//-----------------------------------------------------------------------------

#include <zlib.h>
#include <sys/mman.h>

//-----------------------------------------------------------------------------

QByteArray gZipData(const QByteArray& inputData)
{
    auto fd = memfd_create("gzipThenEncryptData", 0);
    if (fd < 0) {
        return {};
    }

    auto gzFile = gzdopen(fd, "wb");
    if (gzFile == nullptr) {
        close(fd);
        return {};
    }

    if (gzwrite(gzFile, inputData.data(), inputData.length())!=inputData.length()) {
        gzclose(gzFile);
        close(fd);
        return {};
    }

    if (gzflush(gzFile, Z_FINISH)!=Z_OK) {
        close(fd);
        return {};
    }

    auto gzippedSize = lseek(fd, 0, SEEK_CUR);
    if (gzippedSize < 0) {
        close(fd);
        return {};
    }

    if (lseek(fd, 0, SEEK_SET)!=0) {
        close(fd);
        return {};
    }

    QByteArray gZipped(gzippedSize, Qt::Initialization::Uninitialized);

    if (read(fd, gZipped.data(), gzippedSize)!=gzippedSize) {
        close(fd);
        return {};
    }

    close(fd);
    return gZipped;
}

//-----------------------------------------------------------------------------

QByteArray unGzipData(const QByteArray& inputData)
{
    auto fd = memfd_create("decryptThenUnGzipData", 0);
    if (fd < 0) {
        return {};
    }

    if (write(fd, inputData.data(), inputData.length())!=inputData.length()) {
        close(fd);
        return {};
    }

    auto gzippedSize = lseek(fd, 0, SEEK_CUR);
    if (gzippedSize < 0) {
        close(fd);
        return {};
    }

    if (lseek(fd, 0, SEEK_SET)!=0) {
        close(fd);
        return {};
    }

    auto gzFile = gzdopen(fd, "rb");
    if (gzFile == nullptr) {
        close(fd);
        return {};
    }

    QBuffer gunzipBuffer;
    if (!gunzipBuffer.open(QIODevice::WriteOnly)) {
        return {};
    }

    while(!gzeof(gzFile)) {
        char buf[4096];
        auto a = gzread(gzFile, buf, sizeof(buf));
        if (a<0) {
            gunzipBuffer.close();
            gzclose(gzFile);
            return {};
        }

        gunzipBuffer.write(buf, a);
    }

    gzclose(gzFile);
    gunzipBuffer.close();

    return gunzipBuffer.data();
}

//-----------------------------------------------------------------------------
#endif // HAVE_KARCHIVE

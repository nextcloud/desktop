#include "resources.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMimeDatabase>

namespace OCC {

Q_LOGGING_CATEGORY(lcResources, "sync.networkjob.resource")

namespace {
    QString hashMd5(const QString &data)
    {
        QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
        hash.addData(data.toLatin1());
        return QString::fromLatin1(hash.result().toHex());
    }

    QString cacheKey(QNetworkReply *reply)
    {
        const QString urlHash = hashMd5(reply->url().toString());
        const QString eTagHash = hashMd5(reply->header(QNetworkRequest::ETagHeader).toString());

        const auto db = QMimeDatabase();

        QString extension = db.mimeTypeForName(reply->header(QNetworkRequest::ContentTypeHeader).toString()).preferredSuffix();
        if (extension.isEmpty()) {
            extension = db.mimeTypeForData(reply).preferredSuffix();
        }
        if (extension.isEmpty()) {
            extension = QStringLiteral("unknown");
        }

        return QStringLiteral("%1-%2.%3").arg(urlHash, eTagHash, extension);
    }
}

void ResourceJob::finished()
{
    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcResources) << "Network error: " << this << errorString();
    } else {
        _cacheKey = cacheKey(reply());
    }

    // storing the file on disk enables Qt to apply some optimizations, e.g., in QIcon
    // also, specifically for icons, loading from disk allows easy management of scalable and non-scalable entities
    if (httpStatusCode() == 200 && reply()->size() > 0) {
        const QString path = _cache->path(_cacheKey);

        qCDebug(lcResources) << "cache file path:" << path;

        // furthermore, we can skip writing the file if the cache key has not changed (i.e., a file exists) and the file has come from the network cache
        if (QFileInfo::exists(path) && reply()->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool()) {
            qCDebug(lcResources) << "file has come from network cache, skipping writing";
        } else {
            QFile cacheFile(path);

            // note: we want to truncate the file if it exists
            if (!cacheFile.open(QIODevice::WriteOnly)) {
                qCCritical(lcResources) << "failed to open cache file for writing:" << cacheFile.fileName();
            } else {
                if (!cacheFile.write(reply()->readAll())) {
                    qCCritical(lcResources) << "failed to write to cache file:" << cacheFile.fileName();
                }
            }
        }
    }

    SimpleNetworkJob::finished();
}

QIcon ResourceJob::asIcon() const
{
    // storing the file on disk enables Qt to apply some optimizations (e.g., caching of rendered pixmaps)
    Q_ASSERT(!_cacheKey.isEmpty());
    return QIcon(_cache->path(_cacheKey));
}

ResourceJob::ResourceJob(const ResourcesCache *cache, const QUrl &rootUrl, const QString &path, QObject *parent)
    : SimpleNetworkJob(cache->account()->sharedFromThis(), rootUrl, path, "GET", {}, {}, parent)
    , _cache(cache)
{
    setStoreInCache(true);
}

ResourcesCache::ResourcesCache(const QString &cacheDirectory, Account *account)
    : QObject(account)
    , _account(account)
    , _cacheDirectory(QStringLiteral("%1/tmp.XXXXXX").arg(cacheDirectory))
{
    Q_ASSERT(_cacheDirectory.isValid());
}

ResourceJob *ResourcesCache::makeGetJob(const QUrl &rootUrl, const QString &path, QObject *parent) const
{
    return new ResourceJob(this, rootUrl, path, parent);
}

ResourceJob *ResourcesCache::makeGetJob(const QString &path, QObject *parent) const
{
    return makeGetJob(_account->url(), path, parent);
}

Account *ResourcesCache::account() const
{
    return _account;
}

QString ResourcesCache::path(const QString &cacheKey) const
{
    Q_ASSERT(!cacheKey.isEmpty());
    Q_ASSERT(_cacheDirectory.isValid());
    return _cacheDirectory.filePath(cacheKey);
}

}

#pragma once

#include "account.h"
#include "networkjobs.h"

#include <QIcon>
#include <QLoggingCategory>
#include <QTemporaryDir>

namespace OCC {

class ResourcesCache;

/**
 * This job automatically downloads all available data from the server and stores it in a temporary cache directory on the disk.
 * For convenience, a couple of conversion functions are available to convert the binary data to common Qt classes such as QIcon.
 */
class OWNCLOUDSYNC_EXPORT ResourceJob : public SimpleNetworkJob
{
public:
    void finished() override;

    QIcon asIcon() const;

protected:
    explicit ResourceJob(const ResourcesCache *cache, const QUrl &rootUrl, const QString &path, QObject *parent);

private:
    const ResourcesCache *_cache;
    QString _cacheKey;

    friend class ResourcesCache;
};


class OWNCLOUDSYNC_EXPORT ResourcesCache : public QObject
{
    Q_OBJECT

public:
    /**
     *
     * @param cacheDirectory path to temporary cache parent directory (note: this directory must exist)
     * @param account
     */
    explicit ResourcesCache(const QString &cacheDirectory, Account *account);

    ResourceJob *makeGetJob(const QUrl &rootUrl, const QString &path, QObject *parent) const;

    ResourceJob *makeGetJob(const QString &path, QObject *parent) const;

    Account *account() const;

    QString path(const QString &cacheKey) const;

private:
    Account *_account;
    QTemporaryDir _cacheDirectory;
};

} // namespace OCC

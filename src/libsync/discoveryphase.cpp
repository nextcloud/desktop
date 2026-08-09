/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "discoveryphase.h"

#include "common/utility.h"
#include "configfile.h"
#include "discovery.h"
#include "helpers.h"
#include "progressdispatcher.h"
#include "account.h"
#include "clientsideencryptionjobs.h"
#include "foldermetadata.h"

#include "common/asserts.h"
#include "common/checksums.h"
#include "common/filesystembase.h"

#include <csync_exclude.h>
#include "vio/csync_vio_local.h"

#include <QLoggingCategory>
#include <QTimer>
#include <chrono>
#include <utility>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>
#include <cstring>
#include <QDateTime>

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcDiscovery, "nextcloud.sync.discovery", QtInfoMsg)

namespace {

/// Inactivity timeout for the remote directory listings discovery issues.
///
/// AbstractNetworkJob defaults to 300s. That is a reasonable ceiling for propagation, where a
/// stalled transfer costs only itself, but discovery runs a small number of listings through a
/// narrow concurrency budget: a listing the server accepts and then never answers holds its slot
/// and stalls the whole pass behind it. Observed in the field at 148s and still counting, long
/// enough for the connection health check to give up and have the sync torn down.
///
/// The inherited timer measures *inactivity*, not total duration, so this does not put a ceiling
/// on how long a legitimately large listing may take -- one that is still streaming keeps
/// resetting it. Only a listing producing no data at all is cut short.
qint64 discoveryListingTimeoutMsec()
{
    static const auto configuredSec = qEnvironmentVariableIntValue("OWNCLOUD_DISCOVERY_TIMEOUT");
    static constexpr auto defaultSec = 60;
    return qint64{configuredSec > 0 ? configuredSec : defaultSec} * 1000;
}

/// How many times a directory listing is attempted before the lookups waiting on it are failed.
int etagListingMaxAttempts()
{
    static const auto configured = qEnvironmentVariableIntValue("OWNCLOUD_DISCOVERY_LISTING_RETRIES");
    static constexpr auto defaultAttempts = 3;
    return qMax(1, configured > 0 ? configured : defaultAttempts);
}

/// How long a failed attempt counts against a directory's retry budget.
///
/// The budget is meant to survive a bad patch, not to ration retries across an entire sync. A
/// directory that failed a few times half an hour ago and is being tried again now starts over,
/// so attempts spread thinly across a long run never add up to an exhausted budget.
std::chrono::seconds etagListingRetryResetAfter()
{
    static const auto configured = qEnvironmentVariableIntValue("OWNCLOUD_DISCOVERY_LISTING_RETRY_RESET_SEC");
    static constexpr auto defaultSeconds = 300;
    return std::chrono::seconds(qMax(1, configured > 0 ? configured : defaultSeconds));
}

/// Base delay before re-attempting a failed listing; multiplied by the attempt number so a
/// server that is struggling is not immediately hit again with the request that just failed.
std::chrono::seconds etagListingRetryDelay()
{
    static const auto configured = qEnvironmentVariableIntValue("OWNCLOUD_DISCOVERY_LISTING_RETRY_DELAY_SEC");
    static constexpr auto defaultSeconds = 5;
    return std::chrono::seconds(qMax(1, configured > 0 ? configured : defaultSeconds));
}

/// Whether a failed listing is worth sending again.
///
/// Only failures that say "not right now" qualify: no HTTP status at all (timeout, abort,
/// connection dropped), an explicit slow-down, or a server-side error. A 4xx is the server
/// answering the question -- most importantly 404, which is the normal reply when a rename
/// check asks about a directory that has since been renamed away. Retrying those would spend
/// the backoff on every such directory and still arrive at the same answer.
bool isRetryableListingError(const HttpError &error)
{
    if (error.code == 0) {
        return true; // no reply reached us: timeout, abort, or a dropped connection
    }
    if (error.code == 408 || error.code == 429) {
        return true; // request timeout / rate limited
    }
    return error.code >= 500;
}

}

bool DiscoveryPhase::isInSelectiveSyncBlackList(const QString &path) const
{
    if (_selectiveSyncBlackList.isEmpty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // Block if it is in the black list
    if (SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncBlackList, path)) {
        return true;
    }

    return false;
}

bool DiscoveryPhase::activeFolderSizeLimit() const
{
    return _syncOptions._newBigFolderSizeLimit > 0 && _syncOptions._vfs->mode() == Vfs::Off;
}

bool DiscoveryPhase::notifyExistingFolderOverLimit() const
{
    return activeFolderSizeLimit() && ConfigFile().notifyExistingFoldersOverLimit();
}

void DiscoveryPhase::checkFolderSizeLimit(const QString &path, const std::function<void(bool)> completionCallback)
{
    if (!activeFolderSizeLimit()) {
        // no limit, everything is allowed;
        return completionCallback(false);
    }

    // do a PROPFIND to know the size of this folder
    const auto propfindJob = new PropfindJob(_account, _remoteFolder + path, this);
    propfindJob->setProperties(QList<QByteArray>() << "resourcetype"
                                                   << "http://owncloud.org/ns:size");

    connect(propfindJob, &PropfindJob::finishedWithError, this, [=] {
        return completionCallback(false);
    });
    connect(propfindJob, &PropfindJob::result, this, [=, this](const QVariantMap &values) {
        const auto result = values.value(QLatin1String("size")).toLongLong();
        const auto limit = _syncOptions._newBigFolderSizeLimit;
        qCDebug(lcDiscovery) << "Folder size check complete for" << path << "result:" << result << "limit:" << limit;
        return completionCallback(result >= limit);
    });
    propfindJob->start();
}

void DiscoveryPhase::checkSelectiveSyncNewFolder(const QString &path,
                                                 const RemotePermissions remotePerm,
                                                 const std::function<void(bool)> callback)
{
    if (_syncOptions._confirmExternalStorage && _syncOptions._vfs->mode() == Vfs::Off
        && remotePerm.hasPermission(RemotePermissions::IsMounted)) {
        // external storage.

        /* Note: DiscoverySingleDirectoryJob::directoryListingIteratedSlot make sure that only the
         * root of a mounted storage has 'M', all sub entries have 'm' */

        // Only allow it if the white list contains exactly this path (not parents)
        // We want to ask confirmation for external storage even if the parents where selected
        if (_selectiveSyncWhiteList.contains(path + QLatin1Char('/'))) {
            return callback(false);
        }

        emit newBigFolder(path, true);
        return callback(true);
    }

    // If this path or the parent is in the white list, then we do not block this file
    if (SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncWhiteList, path)) {
        return callback(false);
    }

    checkFolderSizeLimit(path, [this, path, callback](const bool bigFolder) {
        if (bigFolder) {
            // we tell the UI there is a new folder
            emit newBigFolder(path, false);
            return callback(true);
        }

        // it is not too big, put it in the white list (so we will not do more query for the children) and and do not block.
        const auto sanitisedPath = Utility::trailingSlashPath(path);
        _selectiveSyncWhiteList.insert(std::upper_bound(_selectiveSyncWhiteList.begin(), _selectiveSyncWhiteList.end(), sanitisedPath), sanitisedPath);
        return callback(false);
    });
}

void DiscoveryPhase::checkSelectiveSyncExistingFolder(const QString &path)
{
    // If no size limit is enforced, or if is in whitelist (explicitly allowed) or in blacklist (explicitly disallowed), do nothing.
    if (!notifyExistingFolderOverLimit() || SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncWhiteList, path)
        || SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncBlackList, path)) {
        return;
    }

    checkFolderSizeLimit(path, [this, path](const bool bigFolder) {
        if (bigFolder) {
            // Notify the user and prompt for response.
            emit existingFolderNowBig(path);
        }
    });
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString DiscoveryPhase::adjustRenamedPath(const QString &original, SyncFileItem::Direction d) const
{
    return OCC::adjustRenamedPath(d == SyncFileItem::Down ? _renamedItemsRemote : _renamedItemsLocal, original);
}

QString adjustRenamedPath(const QMap<QString, QString> &renamedItems, const QString &original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/', slashPos - 1)) > 0) {
        auto it = renamedItems.constFind(original.left(slashPos));
        if (it != renamedItems.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

QPair<bool, QByteArray> DiscoveryPhase::findAndCancelDeletedJob(const QString &originalPath)
{
    bool result = false;
    QByteArray oldEtag;
    auto it = _deletedItem.find(originalPath);
    if (it != _deletedItem.end()) {
        const SyncInstructions instruction = (*it)->_instruction;
        if (instruction == CSYNC_INSTRUCTION_IGNORE && (*it)->_type == ItemTypeVirtualFile) {
            // re-creation of virtual files count as a delete
            // a file might be in an error state and thus gets marked as CSYNC_INSTRUCTION_IGNORE
            // after it was initially marked as CSYNC_INSTRUCTION_REMOVE
            // return true, to not trigger any additional actions on that file that could elad to dataloss
            result = true;
            oldEtag = (*it)->_etag;
        } else {
            if (!(instruction == CSYNC_INSTRUCTION_REMOVE ||
                  instruction == CSYNC_INSTRUCTION_IGNORE ||
                  ((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW) ||// re-creation of virtual files count as a delete
                  ((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW))) {
                qCWarning(lcDiscovery) << "ENFORCE(FAILING)" << originalPath;
                qCWarning(lcDiscovery) << "instruction == CSYNC_INSTRUCTION_REMOVE" << (instruction == CSYNC_INSTRUCTION_REMOVE);
                qCWarning(lcDiscovery) << "((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW)"
                                       << ((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW))"
                                       << ((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "instruction" << instruction;
                qCWarning(lcDiscovery) << "(*it)->_type" << (*it)->_type;
                qCWarning(lcDiscovery) << "(*it)->_isRestoration " << (*it)->_isRestoration;
                Q_ASSERT(false);
                emit addErrorToGui(SyncFileItem::Status::FatalError, tr("Error while canceling deletion of a file"), originalPath, ErrorCategory::GenericError);
                emit fatalError(tr("Error while canceling deletion of %1").arg(originalPath), ErrorCategory::GenericError);
            }
            (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
            result = true;
            oldEtag = (*it)->_etag;
        }
        _deletedItem.erase(it);
    }
    if (auto *otherJob = _queuedDeletedDirectories.take(originalPath)) {
        oldEtag = otherJob->_dirItem->_etag;
        delete otherJob;
        result = true;
    }
    return { result, oldEtag };
}

void DiscoveryPhase::enqueueDirectoryToDelete(const QString &path, ProcessDirectoryJob* const directoryJob)
{
    _queuedDeletedDirectories[path] = directoryJob;

    if (directoryJob->_dirItem &&
        directoryJob->_dirItem->_isRestoration &&
        directoryJob->_dirItem->_direction == SyncFileItem::Down &&
        directoryJob->_dirItem->_instruction == CSYNC_INSTRUCTION_NEW) {

        _directoryNamesToRestoreOnPropagation.push_back(path);
    }
}

bool DiscoveryPhase::recursiveCheckForDeletedParents(const QString &itemPath) const
{
    const auto &allKeys = _deletedItem.keys();
    qCDebug(lcDiscovery()) << allKeys.join(", ");

    auto result = false;
    const auto &pathElements = itemPath.split('/');
    auto currentParentFolder = QString{};
    for (const auto &onePathComponent : pathElements) {
        if (!currentParentFolder.isEmpty()) {
            currentParentFolder += '/';
        }
        currentParentFolder += onePathComponent;

        qCDebug(lcDiscovery()) << "checks" << currentParentFolder << "for" << allKeys.join(", ");
        if (_deletedItem.find(currentParentFolder) == _deletedItem.end()) {
            continue;
        }

        qCDebug(lcDiscovery()) << "deleted parent found";
        result = true;
        break;
    }

    return result;
}

void DiscoveryPhase::markPermanentDeletionRequests()
{
    // since we don't know in advance which files/directories need to be permanently deleted,
    // we have to look through all of them at the end of the run
    for (const auto &originalPath : std::as_const(_permanentDeletionRequests)) {
        const auto it = _deletedItem.find(originalPath);
        if (it == _deletedItem.end()) {
            qCWarning(lcDiscovery) << "didn't find an item for" << originalPath << "(yet)";
            continue;
        }

        auto item = *it;
        if (!(item->_instruction == CSYNC_INSTRUCTION_REMOVE || item->_direction == SyncFileItem::Up)) {
            qCInfo(lcDiscovery) << "will not request permanent deletion for" << originalPath << "as the instruction is not CSYNC_INSTRUCTION_REMOVE, or the direction is not Up";
            continue;
        }

        qCDebug(lcDiscovery) << "requested permanent server-side deletion for" << originalPath;
        item->_wantsSpecificActions = SyncFileItem::SynchronizationOptions::WantsPermanentDeletion;
    }
}

void DiscoveryPhase::startJob(ProcessDirectoryJob *job)
{
    Q_ASSERT(!_currentRootJob);
    connect(this, &DiscoveryPhase::itemDiscovered, this, &DiscoveryPhase::slotItemDiscovered, Qt::UniqueConnection);
    connect(job, &ProcessDirectoryJob::finished, this, [this, job] {
        Q_ASSERT(_currentRootJob == sender());
        _currentRootJob = nullptr;
        if (job->_dirItem)
            emit itemDiscovered(job->_dirItem);
        job->deleteLater();

        // Once the main job has finished recurse here to execute the remaining
        // jobs for queued deleted directories.
        if (!_queuedDeletedDirectories.isEmpty()) {
            auto nextJob = _queuedDeletedDirectories.take(_queuedDeletedDirectories.firstKey());
            startJob(nextJob);
        } else {
            markPermanentDeletionRequests();
            emit finished();
        }
    });
    _currentRootJob = job;
    job->start();
}

void DiscoveryPhase::setSelectiveSyncBlackList(const QStringList &list)
{
    _selectiveSyncBlackList = list;
    _selectiveSyncBlackList.sort();
}

void DiscoveryPhase::setSelectiveSyncWhiteList(const QStringList &list)
{
    _selectiveSyncWhiteList = list;
    _selectiveSyncWhiteList.sort();
}

bool DiscoveryPhase::isRenamed(const QString &p) const
{
    return _renamedItemsLocal.contains(p) || _renamedItemsRemote.contains(p);
}

int DiscoveryPhase::networkJobBudget() const
{
    // Qt's HTTP/1.1 backend opens at most this many connections per host. It is not
    // configurable through the public API, so discovery has to stay under it deliberately.
    static constexpr auto httpConnectionsPerHost = 6;
    // Kept free for account health checks (ConnectionValidator, UserInfo) and other non-sync
    // traffic, so they never wait behind a full set of in-flight discovery requests.
    static constexpr auto connectionsReservedForHealthChecks = 2;

    const auto configured = qMax(1, _syncOptions._parallelNetworkJobs);
    return qMax(1, qMin(configured, httpConnectionsPerHost - connectionsReservedForHealthChecks));
}

void DiscoveryPhase::scheduleMoreJobs()
{
    auto networkLimit = networkJobBudget();
    auto localLimit = qMax(1, _syncOptions._parallelLocalScanJobs);

    // Drain pending rename checks first. They share the network budget, and a caller that
    // frees a slot here would otherwise leave them queued until the next completion.
    startQueuedEtagJobs();

    if (_currentRootJob) {
        int networkBudget = qMax(0, networkLimit - _currentlyActiveJobs);
        int localBudget = qMax(0, localLimit - _currentlyActiveLocalScanJobs);
        if (networkBudget > 0 || localBudget > 0) {
            _currentRootJob->processSubJobs(localBudget, networkBudget);
        }
    }
}

void DiscoveryPhase::enqueueEtagJob(AbstractNetworkJob *job)
{
    _queuedEtagJobs.push_back(job);
    startQueuedEtagJobs();
}

void DiscoveryPhase::startQueuedEtagJobs()
{
    const auto networkLimit = networkJobBudget();
    while (_currentlyActiveJobs < networkLimit && !_queuedEtagJobs.empty()) {
        const auto job = _queuedEtagJobs.front();
        _queuedEtagJobs.pop_front();
        if (!job) {
            continue; // destroyed while waiting in the queue
        }

        ++_currentlyActiveJobs;
        // Connected here rather than in enqueueEtagJob() so that a job destroyed while it was
        // still queued -- and therefore never counted -- cannot decrement the budget.
        //
        // destroyed() rather than a typed completion signal because it fires exactly once on
        // every path: success, network error, and abort. AbstractNetworkJob deleteLater()s
        // itself once finished, so this is queued and cannot re-enter the loop above.
        connect(job, &QObject::destroyed, this, [this] {
            --_currentlyActiveJobs;
            startQueuedEtagJobs();
            scheduleMoreJobs();
        });
        // Armed here rather than at construction so that time spent waiting for a slot does
        // not count against the request. A job armed at construction can expire before it is
        // ever sent, and AbstractNetworkJob::onTimedOut() deletes a job with no reply outright
        // instead of reporting an error.
        job->setTimeout(discoveryListingTimeoutMsec());
        job->start();
    }
}

void DiscoveryPhase::deliverEtagLookup(const PendingEtagLookup &lookup, const RemoteDirListing &listing)
{
    if (!lookup.context) {
        return; // the job that asked is gone; answering it would touch freed state
    }
    if (!listing.listingOk) {
        lookup.callback(listing.listingError);
        return;
    }
    const auto it = listing.childEtags.constFind(lookup.relativePath);
    if (it == listing.childEtags.constEnd()) {
        // Not in the parent listing means it is not on the server. Reported as 404 because
        // that is what a per-file PROPFIND returned here, and callers branch on that code.
        lookup.callback(HttpError{404, tr("File not found on server")});
        return;
    }
    lookup.callback(*it);
}

void DiscoveryPhase::deliverPendingEtagLookups(const QString &dirPath, const RemoteDirListing &listing)
{
    // Deliberately off the LsColJob's stack. These callbacks resume discovery -- they can
    // finish a directory job, start new ones, or fail the whole sync -- and running that
    // from inside the job's own finished/error signal re-enters the job while it is still
    // executing and about to deleteLater() itself. Handing the work to the event loop keeps
    // delivery uniform with the cached path, which is already asynchronous.
    const auto waiting = _pendingEtagLookups.take(dirPath);
    if (waiting.isEmpty()) {
        return;
    }
    QTimer::singleShot(0, this, [this, waiting, listing] {
        for (const auto &lookup : waiting) {
            deliverEtagLookup(lookup, listing);
        }
    });
}

void DiscoveryPhase::lookupRemoteEtag(const QString &remoteFullPath,
                                      QObject *context,
                                      std::function<void(const HttpResult<QByteArray> &)> callback)
{
    const auto lastSlash = remoteFullPath.lastIndexOf('/');
    if (lastSlash < 0) {
        qCWarning(lcDiscovery) << "Cannot look up etag without a parent directory" << remoteFullPath;
        QTimer::singleShot(0, context, [callback] { callback(HttpError{0, QStringLiteral("no parent directory")}); });
        return;
    }
    // lastSlash == 0 means the entry sits directly in the remote root ("/AM"), which happens
    // whenever the folder syncs the account root -- left() would give an empty path, so name
    // the root explicitly. Getting this wrong silently fails every top-level rename.
    const auto dirPath = lastSlash == 0 ? QStringLiteral("/") : remoteFullPath.left(lastSlash);
    const PendingEtagLookup lookup{remoteFullPath.mid(lastSlash + 1), context, std::move(callback)};

    // Already listed: answer from cache, but via the event loop. Callers increment
    // _pendingAsyncJobs and then return expecting the reply to land later; running the
    // callback inline here would re-enter their processing while they are still on the stack.
    if (const auto cached = _remoteDirListings.constFind(dirPath); cached != _remoteDirListings.constEnd()) {
        const auto listing = *cached;
        QTimer::singleShot(0, this, [this, lookup, listing] { deliverEtagLookup(lookup, listing); });
        return;
    }

    // A listing for this directory is already on its way: ride along on it.
    if (const auto pending = _pendingEtagLookups.find(dirPath); pending != _pendingEtagLookups.end()) {
        pending->append(lookup);
        return;
    }

    // First lookup in this directory: it pays for the one listing that answers all the rest.
    _pendingEtagLookups[dirPath].append(lookup);
    issueEtagListing(dirPath);
}

void DiscoveryPhase::issueEtagListing(const QString &dirPath)
{
    const auto job = new LsColJob(_account, dirPath);
    job->setParent(this); // LsColJob takes no parent argument
    // Only the etag is needed. Asking for the default property set would make the response
    // far larger for directories that can hold thousands of entries.
    job->setProperties({"getetag"});
    // The timeout is armed in startQueuedEtagJobs(), not here. Arming it at construction would
    // have it run while the job is still waiting for a slot, so a job that queued behind slow
    // listings could expire before it was ever sent -- see the destroyed() handler below.

    // Hrefs come back as full URL paths. The listed directory itself is included alongside
    // its children, and it is the only entry that is a prefix of all the others, so the
    // shortest href identifies it -- which avoids having to reconstruct the request URL, and
    // stays correct for a directory that contains a child of its own name (/a/b and /a/b/b).
    const auto entries = std::make_shared<QList<QPair<QString, QByteArray>>>();
    connect(job, &LsColJob::directoryListingIterated, this,
        [entries](const QString &href, const QMap<QString, QString> &properties) {
            entries->append({href, parseEtag(properties.value(QStringLiteral("getetag")).toUtf8().constData())});
        });

    // Exactly one of the three handlers below gets to act, whichever arrives first.
    //
    // The destroyed() one is not redundant. A job can reach its end without emitting either
    // completion signal: AbstractNetworkJob::onTimedOut() deletes the job outright when it
    // times out with no reply yet, which is reachable for a job that expired while queued.
    // When that happened, the lookups parked in _pendingEtagLookups were never delivered, so
    // their callers' _pendingAsyncJobs never dropped and discovery waited forever on a job
    // that no longer existed -- an indefinite hang rather than a failed sync. destroyed()
    // fires on every path, so it is the one backstop that cannot be skipped.
    const auto handled = std::make_shared<bool>(false);

    connect(job, &LsColJob::finishedWithoutError, this, [this, dirPath, entries, handled] {
        if (std::exchange(*handled, true)) {
            return;
        }
        RemoteDirListing listing;
        listing.listingOk = true;
        if (!entries->isEmpty()) {
            auto parentHref = entries->first().first;
            for (const auto &entry : std::as_const(*entries)) {
                if (entry.first.size() < parentHref.size()) {
                    parentHref = entry.first;
                }
            }
            for (const auto &entry : std::as_const(*entries)) {
                if (entry.first.size() <= parentHref.size()) {
                    continue; // the directory itself, not a child
                }
                listing.childEtags.insert(entry.first.mid(parentHref.size() + 1), entry.second);
            }
        }
        _etagListingRetries.remove(dirPath);
        _remoteDirListings.insert(dirPath, listing);
        deliverPendingEtagLookups(dirPath, listing);
    });

    connect(job, &LsColJob::finishedWithError, this, [this, dirPath, handled](QNetworkReply *reply) {
        if (std::exchange(*handled, true)) {
            return;
        }
        const auto httpCode = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
        failEtagListing(dirPath, HttpError{httpCode, reply ? reply->errorString() : QString{}});
    });

    connect(job, &QObject::destroyed, this, [this, dirPath, handled] {
        if (std::exchange(*handled, true)) {
            return;
        }
        qCWarning(lcDiscovery) << "Listing job for" << dirPath
                               << "was destroyed without reporting a result; treating as a failure";
        failEtagListing(dirPath, HttpError{0, tr("Directory listing did not complete")});
    });

    // Through the same throttle as everything else discovery sends, so coalescing reduces
    // the request count without changing how many are in flight at once.
    enqueueEtagJob(job);
}

void DiscoveryPhase::failEtagListing(const QString &dirPath, const HttpError &error)
{
    if (!isRetryableListingError(error)) {
        // The server answered; sending the same request again would only get the same answer.
        _etagListingRetries.remove(dirPath);
        RemoteDirListing listing;
        listing.listingOk = false;
        listing.listingError = error;
        _remoteDirListings.insert(dirPath, listing);
        deliverPendingEtagLookups(dirPath, listing);
        return;
    }

    auto &retry = _etagListingRetries[dirPath];

    // Attempts only add up while they keep failing close together. One that follows a long gap
    // starts a fresh budget rather than inheriting what was spent earlier in the run.
    const auto resetAfter = etagListingRetryResetAfter();
    if (retry.sinceLastAttempt.isValid() && retry.sinceLastAttempt.durationElapsed() > resetAfter) {
        retry.attempts = 0;
    }
    ++retry.attempts;
    retry.sinceLastAttempt.start();

    const auto maxAttempts = etagListingMaxAttempts();
    if (retry.attempts < maxAttempts) {
        const auto delay = etagListingRetryDelay() * retry.attempts;
        qCInfo(lcDiscovery) << "Listing" << dirPath << "failed (attempt" << retry.attempts << "of"
                            << maxAttempts << "):" << error.message << "-- retrying in"
                            << delay.count() << "s";
        QTimer::singleShot(delay, this, [this, dirPath] { issueEtagListing(dirPath); });
        return;
    }

    qCWarning(lcDiscovery) << "Listing" << dirPath << "failed" << retry.attempts
                           << "times in a row; giving up:" << error.message;
    _etagListingRetries.remove(dirPath);

    RemoteDirListing listing;
    listing.listingOk = false;
    listing.listingError = error;
    // Cached as a failure too: without this, every one of the ~90 lookups that directory
    // is about to receive would retry the same failing listing.
    _remoteDirListings.insert(dirPath, listing);
    deliverPendingEtagLookups(dirPath, listing);
}

void DiscoveryPhase::slotItemDiscovered(const OCC::SyncFileItemPtr &item)
{
    if (item->_instruction == CSYNC_INSTRUCTION_ERROR && item->_direction == SyncFileItem::Up) {
        _hasUploadErrorItems = true;
    }
    if (item->_instruction == CSYNC_INSTRUCTION_REMOVE && item->_direction == SyncFileItem::Down) {
        _hasDownloadRemovedItems = true;
    }
}

DiscoverySingleLocalDirectoryJob::DiscoverySingleLocalDirectoryJob(const AccountPtr &account,
                                                                   const QString &localPath,
                                                                   OCC::Vfs *vfs,
                                                                   bool fileSystemReliablePermissions,
                                                                   QObject *parent)
    : QObject{parent}
    , QRunnable{}
    , _localPath{localPath}
    , _account{account}
    , _vfs{vfs}
    , _fileSystemReliablePermissions{fileSystemReliablePermissions}
{
    qRegisterMetaType<QVector<OCC::LocalInfo> >("QVector<OCC::LocalInfo>");
}

// Use as QRunnable
void DiscoverySingleLocalDirectoryJob::run() {
    QString localPath = _localPath;
    if (localPath.endsWith('/')) // Happens if _currentFolder._local.isEmpty()
        localPath.chop(1);

    auto dh = csync_vio_local_opendir(localPath);
    if (!dh) {
        qCInfo(lcDiscovery) << "Error while opening directory" << (localPath) << errno;
        QString errorString = tr("Error while opening directory %1").arg(localPath);
        if (errno == EACCES) {
            errorString = tr("Directory not accessible on client, permission denied");
            emit finishedNonFatalError(errorString);
            return;
        } else if (errno == ENOENT) {
            errorString = tr("Directory not found: %1").arg(localPath);
        } else if (errno == ENOTDIR) {
            // Not a directory..
            // Just consider it is empty
            emit finished(QVector<LocalInfo>{});
            return;
        }
        emit finishedFatalError(errorString);
        return;
    }

    QVector<LocalInfo> results;
    while (true) {
        errno = 0;
        auto dirent = csync_vio_local_readdir(dh, _vfs, _fileSystemReliablePermissions);
        if (!dirent)
            break;
        if (dirent->type == ItemTypeSkip)
            continue;
        LocalInfo i;
        static QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        ASSERT(codec);
        QTextCodec::ConverterState state;
        i.name = codec->toUnicode(dirent->path, dirent->path.size(), &state);
        if (state.invalidChars > 0 || state.remainingChars > 0) {
            emit childIgnored(true);
            auto item = SyncFileItemPtr::create();
            //item->_file = _currentFolder._target + i.name;
            // FIXME ^^ do we really need to use _target or is local fine?
            item->_file = _localPath + i.name;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_status = SyncFileItem::NormalError;
            item->_errorString = tr("Filename encoding is not valid");
            emit itemDiscovered(item);
            continue;
        }
        i.modtime = dirent->modtime;
        i.size = dirent->size;
        i.inode = dirent->inode;
        i.isDirectory = dirent->type == ItemTypeDirectory || dirent->type == ItemTypeVirtualDirectory;
        i.isHidden = dirent->is_hidden;
        i.isSymLink = dirent->type == ItemTypeSoftLink;
        i.isVirtualFile = dirent->type == ItemTypeVirtualFile || dirent->type == ItemTypeVirtualFileDownload;
        i.isMetadataMissing = dirent->is_metadata_missing;
        i.isPermissionsInvalid = dirent->isPermissionsInvalid;
        i.type = dirent->type;

        // Access lock state on the worker thread so a blocking open cannot freeze the GUI #10464
        if (!i.isSymLink && !i.isVirtualFile && !i.isDirectory) {
            const QString absoluteLocalPath = localPath + QLatin1Char('/') + i.name;
            i.isLocked = FileSystem::isFileLocked(absoluteLocalPath, FileSystem::LockMode::SharedRead);
            qCDebug(lcDiscovery) << "File" << absoluteLocalPath << "isLocked" << i.isLocked;
        }

        results.push_back(i);
    }
    if (errno != 0) {
        csync_vio_local_closedir(dh);

        // Note: Windows vio converts any error into EACCES
        qCWarning(lcDiscovery) << "readdir failed for file in " << localPath << " - errno: " << errno;
        emit finishedFatalError(tr("Error while reading directory %1").arg(localPath));
        return;
    }

    errno = 0;
    csync_vio_local_closedir(dh);
    if (errno != 0) {
        qCWarning(lcDiscovery) << "closedir failed for file in " << localPath << " - errno: " << errno;
    }

    emit finished(results);
}

DiscoverySingleDirectoryJob::DiscoverySingleDirectoryJob(const AccountPtr &account,
                                                         const QString &path,
                                                         const QString &remoteRootFolderPath,
                                                         const QSet<QString> &topLevelE2eeFolderPaths,
                                                         SyncFileItem::EncryptionStatus parentEncryptionStatus,
                                                         QObject *parent)
    : QObject(parent)
    , _subPath(remoteRootFolderPath + path)
    , _remoteRootFolderPath(remoteRootFolderPath)
    , _account(account)
    , _encryptionStatusCurrent{parentEncryptionStatus}
    , _topLevelE2eeFolderPaths(topLevelE2eeFolderPaths)
{
    Q_ASSERT(!_remoteRootFolderPath.isEmpty());
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    auto *lsColJob = new LsColJob(_account, _subPath);

    const auto props = LsColJob::defaultProperties(_isRootPath ? LsColJob::FolderType::RootFolder : LsColJob::FolderType::ChildFolder,
                                                   _account);
    lsColJob->setProperties(props);
    lsColJob->setTimeout(discoveryListingTimeoutMsec());

    QObject::connect(lsColJob, &LsColJob::directoryListingIterated,
        this, &DiscoverySingleDirectoryJob::directoryListingIteratedSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithoutError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot);
    lsColJob->start();

    _lsColJob = lsColJob;
}

void DiscoverySingleDirectoryJob::abort()
{
    if (_lsColJob && _lsColJob->reply()) {
        _lsColJob->reply()->abort();
    }
}

bool DiscoverySingleDirectoryJob::isFileDropDetected() const
{
    return _isFileDropDetected;
}

bool DiscoverySingleDirectoryJob::encryptedMetadataNeedUpdate() const
{
    return _encryptedMetadataNeedUpdate;
}

SyncFileItem::EncryptionStatus DiscoverySingleDirectoryJob::currentEncryptionStatus() const
{
    return _encryptionStatusCurrent;
}

SyncFileItem::EncryptionStatus DiscoverySingleDirectoryJob::requiredEncryptionStatus() const
{
    return _encryptionStatusRequired;
}

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(const QString &file, const QMap<QString, QString> &map)
{
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (map.contains("permissions")) {
            const auto perm = RemotePermissions::fromServerString(map.value("permissions"),
                                                            _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                                            map);
            emit firstDirectoryPermissions(perm);
            _isExternalStorage = perm.hasPermission(RemotePermissions::IsMounted);
        }
        if (map.contains("data-fingerprint"_L1)) {
            _dataFingerprint = map.value("data-fingerprint"_L1).toUtf8();
            if (_dataFingerprint.isEmpty()) {
                // Placeholder that means that the server supports the feature even if it did not set one.
                _dataFingerprint = "[empty]";
            }
        }
        if (map.contains("fileid"_L1)) {
            // this is from the "oc:fileid" property, this is the plain ID without any special format (e.g. "2")
            _localFileId = map.value("fileid"_L1).toUtf8();

            bool ok = false;
            if (qint64 numericFileId = _localFileId.toLongLong(&ok, 10); ok) {
                qCDebug(lcDiscovery).nospace() << "received numericFileId=" << numericFileId;
                emit firstDirectoryFileId(numericFileId);
            } else {
                qCWarning(lcDiscovery).nospace() << "conversion to qint64 failed _localFileId=" << _localFileId;
            }
        }
        if (map.contains("id"_L1)) {
            // this is from the "oc:id" property, the format is e.g. "00000002oc123xyz987e"
            _fileId = map.value("id"_L1).toUtf8();
        }
        if (map.contains("is-encrypted"_L1) && map.value("is-encrypted"_L1) == "1"_L1) {
            _encryptionStatusCurrent = SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0;
            Q_ASSERT(!_fileId.isEmpty());
        }
        if (map.contains("size"_L1)) {
            _size = map.value("size"_L1).toInt();
        }

        // all folders will contain both
        if (map.contains(FolderQuota::usedBytesC) && map.contains(FolderQuota::availableBytesC)) {          
            // The server can respond with e.g. "2.58440798353E+12" for the quota
            // therefore: parse the string as a double and cast it to i64
            auto ok = false;
            auto quotaValue = static_cast<int64_t>(map.value(FolderQuota::usedBytesC).toDouble(&ok));
            _folderQuota.bytesUsed = ok ? quotaValue : -1;
            quotaValue = static_cast<int64_t>(map.value(FolderQuota::availableBytesC).toDouble(&ok));
            _folderQuota.bytesAvailable = ok ? quotaValue : -1;

            qCDebug(lcDiscovery) << "Setting quota for" << file
                                 << "bytesUsed:" << _folderQuota.bytesUsed
                                 << "bytesAvailable:" << _folderQuota.bytesAvailable
                                 << "ok:" << ok;
            emit setfolderQuota(_folderQuota);
        }
    } else {
        RemoteInfo result;
        int slash = file.lastIndexOf(u'/');
        result.name = file.mid(slash + 1);
        result.size = -1;
        LsColJob::propertyMapToRemoteInfo(map,
                                          _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                          result);
        if (result.isDirectory) {
            result.size = 0;
        }

        _results.push_back(std::move(result));
    }

    //This works in concerto with the RequestEtagJob and the Folder object to check if the remote folder changed.
    if (map.contains("getetag"_L1)) {
        if (_firstEtag.isEmpty()) {
            _firstEtag = parseEtag(map.value("getetag"_L1).toUtf8()); // for directory itself
        }
    }
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot()
{
    if (!_ignoredFirst) {
        // This is a sanity check, if we haven't _ignoredFirst then it means we never received any directoryListingIteratedSlot
        // which means somehow the server XML was bogus
        emit finished(HttpError{ 0, tr("Server error: PROPFIND reply is not XML formatted!") });
        deleteLater();
        return;
    } else if (!_error.isEmpty()) {
        emit finished(HttpError{ 0, _error });
        deleteLater();
        return;
    } else if (isE2eEncrypted() && _account->capabilities().clientSideEncryptionAvailable()) {
        emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
        fetchE2eMetadata();
        return;
    }
    emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
    emit finished(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *reply)
{
    const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto invalidContentType = !contentType.contains("application/xml; charset=utf-8") &&
                                    !contentType.contains("application/xml; charset=\"utf-8\"") &&
                                    !contentType.contains("text/xml; charset=utf-8") &&
                                    !contentType.contains("text/xml; charset=\"utf-8\"");
    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto errorString = _lsColJob->errorString();

    qCWarning(lcDiscovery) << "LSCOL job error" << reply->errorString() << httpCode << reply->error();

    if (reply->error() == QNetworkReply::NoError && invalidContentType) {
        errorString = tr("The server returned an unexpected response that couldn’t be read. Please reach out to your server administrator.”");
        qCWarning(lcDiscovery) << "Server error: PROPFIND reply is not XML formatted!";
    }

    if (reply->error() == QNetworkReply::ContentAccessDenied) {
        emit _account->termsOfServiceNeedToBeChecked();
    }

    emit finished(HttpError{ httpCode, errorString });
    deleteLater();
}

void DiscoverySingleDirectoryJob::fetchE2eMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _localFileId);
    connect(job, &GetMetadataApiJob::jsonReceived,
            this, &DiscoverySingleDirectoryJob::metadataReceived);
    connect(job, &GetMetadataApiJob::error,
            this, &DiscoverySingleDirectoryJob::metadataError);
    job->start();
}

void DiscoverySingleDirectoryJob::metadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcDiscovery) << "Metadata received, applying it to the result list";
    Q_ASSERT(_subPath.startsWith(u'/'));

    const auto job = qobject_cast<GetMetadataApiJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCDebug(lcDiscovery) << "metadataReceived must be called from GetMetadataApiJob's signal";
        emit finished(HttpError{0, tr("Encrypted metadata setup error!")});
        deleteLater();
        return;
    }

    // as per E2EE V2, top level folder is the only source of encryption keys and users that have access to it
    // hence, we need to find its path and pass to any subfolder's metadata, so it will fetch the top level metadata when needed
    // see https://github.com/nextcloud/end_to_end_encryption_rfc/blob/v2.1/RFC.md
    QString topLevelFolderPath = u"/"_s;
    for (const QString &topLevelPath : std::as_const(_topLevelE2eeFolderPaths)) {
        if (_subPath == topLevelPath) {
            topLevelFolderPath = u"/"_s;
            break;
        }
        if (_subPath.startsWith(topLevelPath + u'/')) {
            const auto topLevelPathSplit = topLevelPath.split(u'/');
            topLevelFolderPath = topLevelPathSplit.join(u'/');
            break;
        }
    }

    const auto jsonMetadata = statusCode == 404 ? QByteArray{} : json.toJson(QJsonDocument::Compact);
    const auto jsonMetadataVersion = FolderMetadata::setupVersionFromExistingMetadata(jsonMetadata);
    switch (jsonMetadataVersion) {
    case FolderMetadata::MetadataVersion::VersionUndefined:
    case FolderMetadata::MetadataVersion::Version1:
    case FolderMetadata::MetadataVersion::Version1_2:
        break;
    case FolderMetadata::MetadataVersion::Version2_0:
    case FolderMetadata::MetadataVersion::Version2_1:
        if (job->signature().isEmpty()) {
            qCDebug(lcDiscovery) << "Initial signature is empty.";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            emit finished(HttpError{0, tr("Encrypted metadata setup error: initial signature from server is empty.")});
            deleteLater();
            return;
        }
        break;
    }

    const auto rootEncryptedFolderInfo = RootEncryptedFolderInfo{Utility::fullRemotePathToRemoteSyncRootRelative(topLevelFolderPath, _remoteRootFolderPath)};

    const auto folderType = _topLevelE2eeFolderPaths.contains(_subPath) ? FolderMetadata::FolderType::Root : FolderMetadata::FolderType::Nested;
    Q_ASSERT((folderType == FolderMetadata::FolderType::Root) == (rootEncryptedFolderInfo.path == QStringLiteral("/")));

    const auto e2EeFolderMetadata = new FolderMetadata(_account,
                                                       _remoteRootFolderPath,
                                                       jsonMetadata,
                                                       rootEncryptedFolderInfo,
                                                       job->signature(),
                                                       folderType);
    connect(e2EeFolderMetadata, &FolderMetadata::setupComplete, this, [this, e2EeFolderMetadata] {
        e2EeFolderMetadata->deleteLater();
        if (!e2EeFolderMetadata->isValid()) {
            emit finished(HttpError{0, tr("Encrypted metadata setup error!")});
            deleteLater();
            return;
        }
        _isFileDropDetected = e2EeFolderMetadata->isFileDropPresent();
        _encryptedMetadataNeedUpdate = e2EeFolderMetadata->encryptedMetadataNeedUpdate();
        _encryptionStatusRequired = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
        _encryptionStatusCurrent = e2EeFolderMetadata->existingMetadataEncryptionStatus();

        Q_ASSERT(_encryptionStatusCurrent != SyncFileItem::EncryptionStatus::Encrypted);
        Q_ASSERT(_encryptionStatusCurrent != SyncFileItem::EncryptionStatus::NotEncrypted);

        const auto encryptedFiles = e2EeFolderMetadata->files();

        const auto findEncryptedFile = [=](const QString &name) {
            const auto it = std::find_if(std::cbegin(encryptedFiles), std::cend(encryptedFiles), [=](const FolderMetadata::EncryptedFile &file) {
                return file.encryptedFilename == name;
            });
            if (it == std::cend(encryptedFiles)) {
                return Optional<FolderMetadata::EncryptedFile>();
            } else {
                return Optional<FolderMetadata::EncryptedFile>(*it);
            }
        };

        std::transform(std::cbegin(_results), std::cend(_results), std::begin(_results), [=, this](const RemoteInfo &info) {
            auto result = info;
            const auto encryptedFileInfo = findEncryptedFile(result.name);
            if (encryptedFileInfo) {
                result._isE2eEncrypted = true;
                result.e2eMangledName = _subPath.mid(1) + u'/' + result.name;
                result.name = encryptedFileInfo->originalFilename;
            }
            return result;
        });

        emit finished(_results);
        deleteLater();
    });
}

void DiscoverySingleDirectoryJob::metadataError(const QByteArray &fileId, int httpReturnCode)
{
    qCWarning(lcDiscovery) << "E2EE Metadata job error. Trying to proceed without it." << fileId << httpReturnCode;
    emit finished(_results);
    deleteLater();
}
}

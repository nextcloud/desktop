/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsearchpeoplemodel.h"

#include "account.h"
#include "accountstate.h"
#include "common/utility.h"
#include "networkjobs.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrlQuery>

namespace
{
constexpr auto maximumPeopleResults = 50;

QString avatarUrl(const OCC::AccountPtr &account, const QString &userId)
{
    const auto encodedUserId = QString::fromUtf8(QUrl::toPercentEncoding(userId));
    const auto avatarPath = QStringLiteral("index.php/avatar/%1/64").arg(encodedUserId);
    return OCC::Utility::concatUrlPath(account->url(), avatarPath).toString();
}
}

namespace OCC
{
UnifiedSearchPeopleModel::UnifiedSearchPeopleModel(QObject *parent, int debounceInterval)
    : QAbstractListModel(parent)
{
    _debounceTimer.setSingleShot(true);
    _debounceTimer.setInterval(debounceInterval);
    connect(&_debounceTimer, &QTimer::timeout, this, &UnifiedSearchPeopleModel::startSearch);
}

UnifiedSearchPeopleModel::~UnifiedSearchPeopleModel()
{
    cancel();
}

QVariant UnifiedSearchPeopleModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid));
    const auto &person = _people.at(index.row());
    switch (role) {
    case UserIdRole:
        return person.id;
    case DisplayNameRole:
        return person.displayName;
    case AvatarUrlRole:
        return person.avatarUrl;
    }
    return {};
}

int UnifiedSearchPeopleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _people.size();
}

QHash<int, QByteArray> UnifiedSearchPeopleModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {UserIdRole, "userId"},
        {DisplayNameRole, "displayName"},
        {AvatarUrlRole, "avatarUrl"},
    };
    return roles;
}

AccountState *UnifiedSearchPeopleModel::accountState() const
{
    return _accountState.data();
}

QString UnifiedSearchPeopleModel::searchTerm() const
{
    return _searchTerm;
}

bool UnifiedSearchPeopleModel::busy() const
{
    return _busy;
}

QString UnifiedSearchPeopleModel::errorString() const
{
    return _errorString;
}

void UnifiedSearchPeopleModel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) {
        return;
    }
    cancel();
    clearPeople();
    if (_accountState) {
        disconnect(_accountState, nullptr, this, nullptr);
    }
    _accountState = accountState;
    if (_accountState) {
        connect(_accountState, &AccountState::isConnectedChanged, this, [this] {
            if (!_accountState->isConnected()) {
                cancel();
                clearPeople();
                setErrorString(tr("People search is unavailable."));
            } else {
                setErrorString({});
                _debounceTimer.start(0);
            }
        });
        connect(_accountState, &QObject::destroyed, this, [this] {
            cancel();
            clearPeople();
            setErrorString(tr("People search is unavailable."));
            Q_EMIT accountStateChanged();
        });
    }
    Q_EMIT accountStateChanged();
    if (_accountState && _accountState->isConnected()) {
        setErrorString({});
        _debounceTimer.start(0);
    } else {
        setErrorString(tr("People search is unavailable."));
    }
}

void UnifiedSearchPeopleModel::setSearchTerm(const QString &searchTerm)
{
    if (_searchTerm == searchTerm) {
        return;
    }
    _searchTerm = searchTerm;
    Q_EMIT searchTermChanged();
    cancel();
    clearPeople();
    setErrorString({});
    _debounceTimer.start();
}

void UnifiedSearchPeopleModel::retry()
{
    startSearch();
}

void UnifiedSearchPeopleModel::startSearch()
{
    cancel();
    if (!_accountState || !_accountState->account() || !_accountState->isConnected()) {
        setErrorString(tr("People search is unavailable."));
        return;
    }
    const auto account = _accountState->account();
    if (_searchTerm.trimmed().isEmpty()) {
        auto people = QVector<Person>{};
        const auto selfId = account->davUser();
        if (!selfId.isEmpty()) {
            const auto selfName = account->prettyName().isEmpty() ? selfId : account->prettyName();
            people.push_back({selfId, selfName, avatarUrl(account, selfId)});
        }
        replacePeople(std::move(people));
        setErrorString({});
        return;
    }

    const auto generation = ++_generation;
    const auto job = new JsonApiJob(account, QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/sharees"));
    auto query = QUrlQuery{};
    query.addQueryItem(QStringLiteral("search"), _searchTerm);
    query.addQueryItem(QStringLiteral("itemType"), QStringLiteral("file"));
    query.addQueryItem(QStringLiteral("shareType"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("lookup"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("perPage"), QString::number(maximumPeopleResults));
    job->addQueryParams(query);
    _job = job;
    setBusy(true);
    connect(job, &JsonApiJob::jsonReceived, this, [this, generation, job, account](const QJsonDocument &reply, int statusCode) {
        if (generation != _generation || _job != job) {
            return;
        }
        _job.clear();
        setBusy(false);
        if (statusCode != 200) {
            clearPeople();
            setErrorString(tr("Could not load people. Try again."));
            return;
        }
        auto people = QVector<Person>{};
        auto seen = QSet<QString>{};
        const auto ocs = reply.object().value(QStringLiteral("ocs")).toObject();
        const auto data = ocs.value(QStringLiteral("data")).toObject();
        const auto appendUsers = [&people, &seen, &account](const QJsonArray &users) {
            for (const auto &value : users) {
                if (people.size() >= maximumPeopleResults) {
                    break;
                }
                const auto object = value.toObject();
                const auto id = object.value(QStringLiteral("value")).toObject().value(QStringLiteral("shareWith")).toString();
                if (id.isEmpty() || seen.contains(id)) {
                    continue;
                }
                seen.insert(id);
                people.push_back({id, object.value(QStringLiteral("label")).toString(id), avatarUrl(account, id)});
            }
        };
        appendUsers(data.value(QStringLiteral("exact")).toObject().value(QStringLiteral("users")).toArray());
        appendUsers(data.value(QStringLiteral("users")).toArray());
        const auto selfId = account->davUser();
        const auto selfName = account->prettyName().isEmpty() ? selfId : account->prettyName();
        const auto selfMatchesSearch =
            _searchTerm.isEmpty() || selfId.contains(_searchTerm, Qt::CaseInsensitive) || selfName.contains(_searchTerm, Qt::CaseInsensitive);
        if (!selfId.isEmpty() && !seen.contains(selfId) && selfMatchesSearch) {
            people.prepend({selfId, selfName, avatarUrl(account, selfId)});
            if (people.size() > maximumPeopleResults) {
                people.removeLast();
            }
        }
        replacePeople(std::move(people));
        setErrorString({});
    });
    job->start();
}

void UnifiedSearchPeopleModel::cancel()
{
    _debounceTimer.stop();
    ++_generation;
    if (_job) {
        disconnect(_job, nullptr, this, nullptr);
        const auto job = qobject_cast<JsonApiJob *>(_job.data());
        const auto reply = job ? job->reply() : nullptr;
        if (reply && reply->isRunning()) {
            reply->abort();
        }
        _job.clear();
    }
    setBusy(false);
}

void UnifiedSearchPeopleModel::clearPeople()
{
    if (_people.isEmpty()) {
        return;
    }
    beginResetModel();
    _people.clear();
    endResetModel();
}

void UnifiedSearchPeopleModel::replacePeople(QVector<Person> people)
{
    beginResetModel();
    _people = std::move(people);
    endResetModel();
}

void UnifiedSearchPeopleModel::setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    Q_EMIT busyChanged();
}

void UnifiedSearchPeopleModel::setErrorString(const QString &errorString)
{
    if (_errorString == errorString) {
        return;
    }
    _errorString = errorString;
    Q_EMIT errorStringChanged();
}
}

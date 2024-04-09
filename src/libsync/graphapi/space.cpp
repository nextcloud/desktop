/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "space.h"

#include "libsync/account.h"
#include "libsync/graphapi/spacesmanager.h"
#include "libsync/networkjobs.h"
#include "libsync/networkjobs/resources.h"

#include "resources/resources.h"

using namespace OCC;
using namespace GraphApi;

namespace {

const auto personalC = QLatin1String("personal");

// https://github.com/cs3org/reva/blob/0cde0a3735beaa14ebdfd8988c3eb77b3c2ab0e6/pkg/utils/utils.go#L56-L59
const auto sharesIdC = QLatin1String("a0ca6a90-a365-4782-871e-d44447bbc668$a0ca6a90-a365-4782-871e-d44447bbc668");
}

Space::Space(SpacesManager *spacesManager, const OpenAPI::OAIDrive &drive)
    : QObject(spacesManager)
    , _spaceManager(spacesManager)
{
    setDrive(drive);
}

OpenAPI::OAIDrive Space::drive() const
{
    return _drive;
}

void Space::setDrive(const OpenAPI::OAIDrive &drive)
{
    _drive = drive;
    if (!imageUrl().isEmpty()) {
        auto job = _spaceManager->account()->resourcesCache()->makeGetJob(imageUrl(), {}, this);
        connect(job, &SimpleNetworkJob::finishedSignal, this, [job, this] {
            if (job->httpStatusCode() == 200) {
                _image = job->asIcon();
                Q_EMIT _spaceManager->spaceChanged(this);
            }
        });
        job->start();
    }
}
QString Space::displayName() const
{
    if (_drive.getDriveType() == personalC) {
        return tr("Personal");
    } else if (_drive.getId() == sharesIdC) {
        // don't call it ShareJail
        return tr("Shares");
    }
    return _drive.getName();
}

uint32_t Space::priority() const
{
    if (_drive.getDriveType() == personalC) {
        return 100;
    } else if (_drive.getId() == sharesIdC) {
        return 50;
    }
    return 0;
}

bool Space::disabled() const
{
    // this is how disabled spaces are represented in the graph API
    return _drive.getRoot().getDeleted().getState() == QLatin1String("trashed");
}

QUrl Space::imageUrl() const
{
    const auto &special = _drive.getSpecial();
    const auto img = std::find_if(special.cbegin(), special.cend(), [](const auto &it) { return it.getSpecialFolder().getName() == QLatin1String("image"); });
    return img == special.cend() ? QUrl() : QUrl(img->getWebDavUrl());
}

QIcon Space::image() const
{
    if (_image.isNull()) {
        return Resources::getCoreIcon(QStringLiteral("space"));
    }
    return _image;
}

QString Space::id()
{
    return _drive.getRoot().getId();
}

QUrl Space::webdavUrl() const
{
    return QUrl(_drive.getRoot().getWebDavUrl());
}

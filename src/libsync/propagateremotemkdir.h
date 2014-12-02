/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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
#pragma once

#include "owncloudpropagator.h"
#include "networkjobs.h"

namespace OCC {

class PropagateRemoteMkdir : public PropagateItemJob {
    Q_OBJECT
    QPointer<AbstractNetworkJob> _job;
    friend class PropagateDirectory; // So it can access the _item;
public:
    PropagateRemoteMkdir (OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item) {}
    void start() Q_DECL_OVERRIDE;
    void abort() Q_DECL_OVERRIDE;
private slots:
    void slotMkcolJobFinished();
    void propfindResult(const QVariantMap &);
    void propfindError();
};

}
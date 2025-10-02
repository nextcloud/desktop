/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FOLDERQUOTA_H
#define FOLDERQUOTA_H

#include <cstdint>

namespace OCC {

/**
 * Represent the quota for each folder retrieved from the server
 * bytesUsed: space used in bytes
 * bytesAvailale: free space available in bytes or
 *                -1: Uncomputed free space - new folder (externally created) not yet scanned by the server
 *                -2: Unknown free space
 *                -3: Unlimited free space.
 */
struct FolderQuota
{
    int64_t bytesUsed = -1;
    int64_t bytesAvailable = -1;
    enum ServerEntry {
        Invalid = 0,
        Valid
    };
    static constexpr char availableBytesC[] = "quota-available-bytes";
    static constexpr char usedBytesC[] = "quota-used-bytes";
};

}

#endif // FOLDERQUOTA_H

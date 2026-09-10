/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "helpers.h"

namespace OCC
{
QByteArray parseEtag(const char *header)
{
    if (!header) {
        return {};
    }

    QByteArray result = header;

    // Weak ETags can appear when compression is used.
    // https://github.com/owncloud/client/issues/3946
    if (result.startsWith("W/")) {
        result = result.mid(2);
    }

    // Remove any surrounding quotes.
    if (result.length() >= 2 && result.startsWith('"') && result.endsWith('"')) {
        result = result.mid(1, result.length() - 2);
    }

    // Strip the -gzip suffix.
    // https://github.com/owncloud/client/issues/1195
    if (result.endsWith("-gzip")) {
        result.chop(5);
    }

    return result;
}
} // namespace OCC

#include "helpers.h"

namespace OCC
{
QByteArray parseEtag(const char *header)
{
    if (!header) {
        return {};
    }
    QByteArray result = header;

    // Weak E-Tags can appear when gzip compression is on, see #3946
    if (result.startsWith("W/")) {
        result = result.mid(2);
    }

    // https://github.com/owncloud/client/issues/1195
    result.replace("-gzip", "");

    if (result.length() >= 2 && result.startsWith('"') && result.endsWith('"')) {
        result = result.mid(1, result.length() - 2);
    }
    return result;
}
} // namespace OCC
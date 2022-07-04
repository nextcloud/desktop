#pragma once
#include "config.h"

namespace CfApiShellExtensions {
static constexpr auto ThumbnailProviderMainServerName = APPLICATION_NAME":cfApiShellExtensionsServer";

namespace Protocol {
    static constexpr auto ThumbnailProviderRequestKey = "thumbnailProviderRequest";
    static constexpr auto ThumbnailProviderRequestFilePathKey = "filePath";
    static constexpr auto ThumbnailProviderRequestFileSizeKey = "size";
    static constexpr auto ThumbnailProviderServerNameKey = "serverName";
};
}

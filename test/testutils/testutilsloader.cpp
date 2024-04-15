#include "configfile.h"
#include "gui/networkinformation.h"
#include "logger.h"
#include "resources/loadresources.h"
#include "testutils.h"

#include <QCoreApplication>

namespace {
void setUpTests()
{
    // load the resources
    static const OCC::ResourcesLoader resources;

    static auto dir = OCC::TestUtils::createTempDir();
    OCC::ConfigFile::setConfDir(QStringLiteral("%1/config").arg(dir.path())); // we don't want to pollute the user's config file

    OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
    OCC::Logger::instance()->addLogRule({ QStringLiteral("sync.httplogger=true") });
    OCC::Logger::instance()->setLogDebug(true);

    OCC::Account::setCommonCacheDirectory(QStringLiteral("%1/cache").arg(dir.path()));

    // ensure we have an instance of NetworkInformation
    OCC::NetworkInformation::instance();
}
Q_COREAPP_STARTUP_FUNCTION(setUpTests)
}

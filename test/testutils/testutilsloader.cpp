#include "configfile.h"
#include "logger.h"
#include "resources/loadresources.h"
#include "testutils.h"

#include <QCoreApplication>
#include <QTemporaryDir>

namespace {
void setupLogger()
{
    // load the resources
    static const OCC::ResourcesLoader resources;

    static auto dir = OCC::TestUtils::createTempDir();
    OCC::ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file

    OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
    OCC::Logger::instance()->addLogRule({ QStringLiteral("sync.httplogger=true") });
    OCC::Logger::instance()->setLogDebug(true);
}
Q_COREAPP_STARTUP_FUNCTION(setupLogger);
}

#include "loadresources.h"

#include <qglobal.h>

using namespace OCC;

void static load_rc()
{
    Q_INIT_RESOURCE(owncloudResources_translations);
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(core_theme);
#ifdef BRANDING_AVAILABLE
    Q_INIT_RESOURCE(theme);
#endif
}

void static unload_rc()
{
    Q_CLEANUP_RESOURCE(owncloudResources_translations);
    Q_CLEANUP_RESOURCE(client);
    Q_CLEANUP_RESOURCE(core_theme);
#ifdef BRANDING_AVAILABLE
    Q_CLEANUP_RESOURCE(theme);
#endif
}

ResourcesLoader::ResourcesLoader()
{
    // Q_INIT_RESOURCE must not be called in a namespace
    ::load_rc();
}


ResourcesLoader::~ResourcesLoader()
{
    // Q_CLEANUP_RESOURCE must not be called in a namespace
    ::unload_rc();
}
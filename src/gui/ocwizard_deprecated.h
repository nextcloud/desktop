#include <QObject>

/*
 * These functions are all deprecated and were previously located within the wizard class.
 */

namespace OCC::OwncloudWizard {

[[deprecated]] static inline void askExperimentalVirtualFilesFeature(QObject *, std::function<void(bool)>)
{
    qWarning() << "Currently unsupported function askExperimentalVirtualFilesFeature called";
}

[[deprecated]] static inline bool useVirtualFileSync()
{
    qWarning() << "Currently unsupported function useVirtualFileSync called";
    return false;
}

[[deprecated]] static inline QList<QString> selectiveSyncBlacklist()
{
    qWarning() << "Currently unsupported function selectiveSyncBlacklist called";
    return {};
}

[[deprecated]] static inline bool isConfirmBigFolderChecked()
{
    qWarning() << "Currently unsupported function isConfirmBigFolderChecked called";
    return false;
}
}

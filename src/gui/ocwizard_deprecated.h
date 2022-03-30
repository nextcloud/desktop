#include <QObject>

/*
 * These functions are all deprecated and were previously located within the wizard class.
 */

namespace OCC::OwncloudWizard {

[[deprecated]] static inline bool isConfirmBigFolderChecked()
{
    qWarning() << "Currently unsupported function isConfirmBigFolderChecked called";
    return false;
}
}

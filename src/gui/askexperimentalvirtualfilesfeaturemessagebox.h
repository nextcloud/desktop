#pragma once

#include <QMessageBox>

namespace OCC {

/**
 * A simple message box used whenever we have to ask the user whether to enable VFS, which is an experimental feature.
 * The dialog will clean itself up after it has been closed.
 */
class AskExperimentalVirtualFilesFeatureMessageBox : public QMessageBox
{
public:
    explicit AskExperimentalVirtualFilesFeatureMessageBox(QWidget *parent = nullptr);
};

}

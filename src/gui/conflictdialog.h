/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFLICTDIALOG_H
#define CONFLICTDIALOG_H

#include <QDialog>

namespace OCC {

class ConflictSolver;

namespace Ui {
    class ConflictDialog;
}

class ConflictDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConflictDialog(QWidget *parent = nullptr);
    ~ConflictDialog() override;

    [[nodiscard]] QString baseFilename() const;
    [[nodiscard]] QString localVersionFilename() const;
    [[nodiscard]] QString remoteVersionFilename() const;

public slots:
    void setBaseFilename(const QString &baseFilename);
    void setLocalVersionFilename(const QString &localVersionFilename);
    void setRemoteVersionFilename(const QString &remoteVersionFilename);

    void accept() override;

private:
    void updateWidgets();
    void updateButtonStates();

    QString _baseFilename;
    QScopedPointer<Ui::ConflictDialog> _ui;
    ConflictSolver *_solver;
};

} // namespace OCC

#endif // CONFLICTDIALOG_H

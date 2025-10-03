/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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

    QString baseFilename() const;
    QString localVersionFilename() const;
    QString remoteVersionFilename() const;

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

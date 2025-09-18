/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LEGALNOTICE_H
#define LEGALNOTICE_H

#include <QDialog>

namespace OCC {
class IgnoreListEditor;
class SyncLogDialog;

namespace Ui {
    class LegalNotice;
}

/**
 * @brief The LegalNotice class
 * @ingroup gui
 */
class LegalNotice : public QDialog
{
    Q_OBJECT

public:
    explicit LegalNotice(QDialog *parent = nullptr);
    ~LegalNotice() override;

protected:
    void changeEvent(QEvent *) override;

private:
    void customizeStyle();

    Ui::LegalNotice *_ui;
};

} // namespace OCC
#endif // LEGALNOTICE_H

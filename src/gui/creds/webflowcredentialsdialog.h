/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WEBFLOWCREDENTIALSDIALOG_H
#define WEBFLOWCREDENTIALSDIALOG_H

#include "accountfwd.h"
#include "creds/flow2auth.h"

#include <QDialog>

class QLabel;
class QVBoxLayout;

namespace OCC {

class Flow2AuthWidget;

class WebFlowCredentialsDialog : public QDialog
{
    Q_OBJECT
public:
    WebFlowCredentialsDialog(Account *account, QWidget *parent = nullptr);

    void setInfo(const QString &msg);
    void setError(const QString &error);

protected:
    void closeEvent(QCloseEvent * e) override;
    void changeEvent(QEvent *) override;

public slots:
    void slotFlow2AuthResult(OCC::Flow2Auth::Result, const QString &errorString, const QString &user, const QString &appPassword);
    void slotShowSettingsDialog();

signals:
    void urlCatched(const QString user, const QString pass, const QString host);
    void styleChanged();
    void onActivate();
    void onClose();

private:
    void customizeStyle();

    Flow2AuthWidget *_flow2AuthWidget = nullptr;

    QLabel *_errorLabel;
    QLabel *_infoLabel;
    QVBoxLayout *_layout;
    QVBoxLayout *_containerLayout;
};

} // namespace OCC

#endif // WEBFLOWCREDENTIALSDIALOG_H

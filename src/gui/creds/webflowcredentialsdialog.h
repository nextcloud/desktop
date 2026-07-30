/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WEBFLOWCREDENTIALSDIALOG_H
#define WEBFLOWCREDENTIALSDIALOG_H

#include "config.h"

#include "accountfwd.h"
#include "creds/flow2auth.h"

#include <QDialog>
#include <QUrl>

class QLabel;
class QVBoxLayout;

namespace OCC {

#ifdef WITH_WEBENGINE
class WebView;
#endif // WITH_WEBENGINE
class Flow2AuthWidget;

class WebFlowCredentialsDialog : public QDialog
{
    Q_OBJECT
public:
    WebFlowCredentialsDialog(Account *account, bool useFlow2, QWidget *parent = nullptr);

    void setUrl(const QUrl &url);
    void setInfo(const QString &msg);
    void setError(const QString &error);

    [[nodiscard]] bool isUsingFlow2() const {
        return _useFlow2;
    }

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

    bool _useFlow2;

    Flow2AuthWidget *_flow2AuthWidget = nullptr;
#ifdef WITH_WEBENGINE
    WebView *_webView = nullptr;
#endif // WITH_WEBENGINE

    QLabel *_errorLabel;
    QLabel *_infoLabel;
    QVBoxLayout *_layout;
    QVBoxLayout *_containerLayout;
};

} // namespace OCC

#endif // WEBFLOWCREDENTIALSDIALOG_H

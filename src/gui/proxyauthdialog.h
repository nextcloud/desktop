/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCC_PROXYAUTHDIALOG_H
#define OCC_PROXYAUTHDIALOG_H

#include <QDialog>

namespace OCC {

namespace Ui {
    class ProxyAuthDialog;
}

/**
 * @brief Ask for username and password for a given proxy.
 *
 * Used by ProxyAuthHandler.
 */
class ProxyAuthDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProxyAuthDialog(QWidget *parent = nullptr);
    ~ProxyAuthDialog() override;

    void setProxyAddress(const QString &address);

    [[nodiscard]] QString username() const;
    [[nodiscard]] QString password() const;

    /// Resets the dialog for new credential entry.
    void reset();

private:
    Ui::ProxyAuthDialog *ui;
};


} // namespace OCC
#endif // OCC_PROXYAUTHDIALOG_H

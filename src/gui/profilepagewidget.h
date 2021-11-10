#pragma once

#include "ocsprofileconnector.h"

#include <QBoxLayout>
#include <QLabel>
#include <account.h>
#include <QMenu>

#include <cstddef>

namespace OCC {

class ProfilePageMenu : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePageMenu(AccountPtr account, const QString &shareWithUserId, QWidget *parent = nullptr);
    ~ProfilePageMenu() override;

    void exec(const QPoint &globalPosition);

private:
    void onHovercardFetched();
    void onIconLoaded(const std::size_t &hovercardActionIndex);

    OcsProfileConnector _profileConnector;
    QMenu _menu;
};
}

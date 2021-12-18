#include "profilepagewidget.h"
#include "guiutility.h"
#include "ocsprofileconnector.h"

namespace OCC {

ProfilePageMenu::ProfilePageMenu(AccountPtr account, const QString &shareWithUserId, QWidget *parent)
    : QWidget(parent)
    , _profileConnector(account)
{
    connect(&_profileConnector, &OcsProfileConnector::hovercardFetched, this, &ProfilePageMenu::onHovercardFetched);
    connect(&_profileConnector, &OcsProfileConnector::iconLoaded, this, &ProfilePageMenu::onIconLoaded);
    _profileConnector.fetchHovercard(shareWithUserId);
}

ProfilePageMenu::~ProfilePageMenu() = default;

void ProfilePageMenu::exec(const QPoint &globalPosition)
{
    _menu.exec(globalPosition);
}

void ProfilePageMenu::onHovercardFetched()
{
    _menu.clear();

    const auto hovercardActions = _profileConnector.hovercard()._actions;
    for (const auto &hovercardAction : hovercardActions) {
        const auto action = _menu.addAction(hovercardAction._icon, hovercardAction._title);
        const auto link = hovercardAction._link;
        connect(action, &QAction::triggered, action, [link](bool) { Utility::openBrowser(link); });
    }
}

void ProfilePageMenu::onIconLoaded(const std::size_t &hovercardActionIndex)
{
    const auto hovercardActions = _profileConnector.hovercard()._actions;
    const auto menuActions = _menu.actions();
    if (hovercardActionIndex >= hovercardActions.size()
        || hovercardActionIndex >= static_cast<std::size_t>(menuActions.size())) {
        return;
    }
    const auto menuAction = menuActions[static_cast<int>(hovercardActionIndex)];
    menuAction->setIcon(hovercardActions[hovercardActionIndex]._icon);
}
}

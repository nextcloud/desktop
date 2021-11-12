/*
 *  SPDX-FileCopyrightText: 2019 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "wheelhandler.h"
#include <QWheelEvent>
#include <QQuickItem>
#include <QDebug>

class GlobalWheelFilterSingleton
{
public:
    GlobalWheelFilter self;
};

Q_GLOBAL_STATIC(GlobalWheelFilterSingleton, privateGlobalWheelFilterSelf)

GlobalWheelFilter::GlobalWheelFilter(QObject *parent)
    : QObject(parent)
{
}

GlobalWheelFilter::~GlobalWheelFilter() = default;

GlobalWheelFilter *GlobalWheelFilter::self()
{
    return &privateGlobalWheelFilterSelf()->self;
}

void GlobalWheelFilter::setItemHandlerAssociation(QQuickItem *item, WheelHandler *handler)
{
    if (!m_handlersForItem.contains(handler->target())) {
        handler->target()->installEventFilter(this);
    }
    m_handlersForItem.insert(item, handler);

    connect(item, &QObject::destroyed, this, [this](QObject *obj) {
        auto item = static_cast<QQuickItem *>(obj);
        m_handlersForItem.remove(item);
    });

    connect(handler, &QObject::destroyed, this, [this](QObject *obj) {
        auto handler = static_cast<WheelHandler *>(obj);
        removeItemHandlerAssociation(handler->target(), handler);
    });
}

void GlobalWheelFilter::removeItemHandlerAssociation(QQuickItem *item, WheelHandler *handler)
{
    if (!item || !handler) {
        return;
    }
    m_handlersForItem.remove(item, handler);
    if (!m_handlersForItem.contains(item)) {
        item->removeEventFilter(this);
    }
}

bool GlobalWheelFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto item = qobject_cast<QQuickItem *>(watched);
        if (!item || !item->isEnabled()) {
            return QObject::eventFilter(watched, event);
        }
        auto we = static_cast<QWheelEvent *>(event);
        m_wheelEvent.initializeFromEvent(we);

        bool shouldBlock = false;
        bool shouldScrollFlickable = false;

        for (auto *handler : m_handlersForItem.values(item)) {
            if (handler->m_blockTargetWheel) {
                shouldBlock = true;
            }
            if (handler->m_scrollFlickableTarget) {
                shouldScrollFlickable = true;
            }
            emit handler->wheel(&m_wheelEvent);
        }

        if (shouldScrollFlickable && !m_wheelEvent.isAccepted()) {
            manageWheel(item, we);
        }

        if (shouldBlock) {
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void GlobalWheelFilter::manageWheel(QQuickItem *target, QWheelEvent *event)
{
    // Duck typing: accept everyhint that has all the properties we need
    if (target->metaObject()->indexOfProperty("contentX") == -1
        || target->metaObject()->indexOfProperty("contentY") == -1
        || target->metaObject()->indexOfProperty("contentWidth") == -1
        || target->metaObject()->indexOfProperty("contentHeight") == -1
        || target->metaObject()->indexOfProperty("topMargin") == -1
        || target->metaObject()->indexOfProperty("bottomMargin") == -1
        || target->metaObject()->indexOfProperty("leftMargin") == -1
        || target->metaObject()->indexOfProperty("rightMargin") == -1
        || target->metaObject()->indexOfProperty("originX") == -1
        || target->metaObject()->indexOfProperty("originY") == -1) {
        return;
    }

    qreal contentWidth = target->property("contentWidth").toReal();
    qreal contentHeight = target->property("contentHeight").toReal();
    qreal contentX = target->property("contentX").toReal();
    qreal contentY = target->property("contentY").toReal();
    qreal topMargin = target->property("topMargin").toReal();
    qreal bottomMargin = target->property("bottomMargin").toReal();
    qreal leftMargin = target->property("leftMaring").toReal();
    qreal rightMargin = target->property("rightMargin").toReal();
    qreal originX = target->property("originX").toReal();
    qreal originY = target->property("originY").toReal();

    // Scroll Y
    if (contentHeight > target->height()) {

        int y = event->pixelDelta().y() != 0 ? event->pixelDelta().y() : event->angleDelta().y() / 8;

        //if we don't have a pixeldelta, apply the configured mouse wheel lines
        if (!event->pixelDelta().y()) {
            y *= 3; // Magic copied value from Kirigami::Settings
        }

        // Scroll one page regardless of delta:
        if ((event->modifiers() & Qt::ControlModifier) || (event->modifiers() & Qt::ShiftModifier)) {
            if (y > 0) {
                y = target->height();
            } else if (y < 0) {
                y = -target->height();
            }
        }

        qreal minYExtent = topMargin - originY;
        qreal maxYExtent = target->height() - (contentHeight + bottomMargin + originY);

        target->setProperty("contentY", qMin(-maxYExtent, qMax(-minYExtent, contentY - y)));
    }

    //Scroll X
    if (contentWidth > target->width()) {

        int x = event->pixelDelta().x() != 0 ? event->pixelDelta().x() : event->angleDelta().x() / 8;

        // Special case: when can't scroll vertically, scroll horizontally with vertical wheel as well
        if (x == 0 && contentHeight <= target->height()) {
            x = event->pixelDelta().y() != 0 ? event->pixelDelta().y() : event->angleDelta().y() / 8;
        }

        //if we don't have a pixeldelta, apply the configured mouse wheel lines
        if (!event->pixelDelta().x()) {
            x *= 3; // Magic copied value from Kirigami::Settings
        }

        // Scroll one page regardless of delta:
        if ((event->modifiers() & Qt::ControlModifier) || (event->modifiers() & Qt::ShiftModifier)) {
            if (x > 0) {
                x = target->width();
            } else if (x < 0) {
                x = -target->width();
            }
        }

        qreal minXExtent = leftMargin - originX;
        qreal maxXExtent = target->width() - (contentWidth + rightMargin + originX);

        target->setProperty("contentX", qMin(-maxXExtent, qMax(-minXExtent, contentX - x)));
    }

    //this is just for making the scrollbar
    target->metaObject()->invokeMethod(target, "flick", Q_ARG(double, 0), Q_ARG(double, 1));
    target->metaObject()->invokeMethod(target, "cancelFlick");
}


////////////////////////////
KirigamiWheelEvent::KirigamiWheelEvent(QObject *parent)
    : QObject(parent)
{}

KirigamiWheelEvent::~KirigamiWheelEvent() = default;

void KirigamiWheelEvent::initializeFromEvent(QWheelEvent *event)
{
    m_x = event->position().x();
    m_y = event->position().y();
    m_angleDelta = event->angleDelta();
    m_pixelDelta = event->pixelDelta();
    m_buttons = event->buttons();
    m_modifiers = event->modifiers();
    m_accepted = false;
    m_inverted = event->inverted();
}

qreal KirigamiWheelEvent::x() const
{
    return m_x;
}

qreal KirigamiWheelEvent::y() const
{
    return m_y;
}

QPointF KirigamiWheelEvent::angleDelta() const
{
    return m_angleDelta;
}

QPointF KirigamiWheelEvent::pixelDelta() const
{
    return m_pixelDelta;
}

int KirigamiWheelEvent::buttons() const
{
    return m_buttons;
}

int KirigamiWheelEvent::modifiers() const
{
    return m_modifiers;
}

bool KirigamiWheelEvent::inverted() const
{
    return m_inverted;
}

bool KirigamiWheelEvent::isAccepted()
{
    return m_accepted;
}

void KirigamiWheelEvent::setAccepted(bool accepted)
{
    m_accepted = accepted;
}


///////////////////////////////

WheelHandler::WheelHandler(QObject *parent)
    : QObject(parent)
{
}

WheelHandler::~WheelHandler() = default;

QQuickItem *WheelHandler::target() const
{
    return m_target;
}

void WheelHandler::setTarget(QQuickItem *target)
{
    if (m_target == target) {
        return;
    }

    if (m_target) {
        GlobalWheelFilter::self()->removeItemHandlerAssociation(m_target, this);
    }

    m_target = target;

    GlobalWheelFilter::self()->setItemHandlerAssociation(target, this);

    emit targetChanged();
}


#include "moc_wheelhandler.cpp"

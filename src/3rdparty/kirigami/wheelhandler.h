/*
 *  SPDX-FileCopyrightText: 2019 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <QtQml>
#include <QPoint>
#include <QQuickItem>
#include <QObject>

class QWheelEvent;

class WheelHandler;

/**
 * Describes the mouse wheel event
 */
class KirigamiWheelEvent : public QObject
{
    Q_OBJECT

    /**
     * x: real
     *
     * X coordinate of the mouse pointer
     */
    Q_PROPERTY(qreal x READ x CONSTANT)

    /**
     * y: real
     *
     * Y coordinate of the mouse pointer
     */
    Q_PROPERTY(qreal y READ y CONSTANT)

    /**
     * angleDelta: point
     *
     * The distance the wheel is rotated in degrees.
     * The x and y coordinates indicate the horizontal and vertical wheels respectively.
     * A positive value indicates it was rotated up/right, negative, bottom/left
     * This value is more likely to be set in traditional mice.
     */
    Q_PROPERTY(QPointF angleDelta READ angleDelta CONSTANT)

    /**
     * pixelDelta: point
     *
     * provides the delta in screen pixels available on high resolution trackpads
     */
    Q_PROPERTY(QPointF pixelDelta READ pixelDelta CONSTANT)

    /**
     * buttons: int
     *
     * it contains an OR combination of the buttons that were pressed during the wheel, they can be:
     * Qt.LeftButton, Qt.MiddleButton, Qt.RightButton
     */
    Q_PROPERTY(int buttons READ buttons CONSTANT)

    /**
     * modifiers: int
     *
     * Keyboard mobifiers that were pressed during the wheel event, such as:
     * Qt.NoModifier (default, no modifiers)
     * Qt.ControlModifier
     * Qt.ShiftModifier
     * ...
     */
    Q_PROPERTY(int modifiers READ modifiers CONSTANT)

    /**
     * inverted: bool
     *
     * Whether the delta values are inverted
     * On some platformsthe returned delta are inverted, so positive values would mean bottom/left
     */
    Q_PROPERTY(bool inverted READ inverted CONSTANT)

    /**
     * accepted: bool
     *
     * If set, the event shouldn't be managed anymore,
     * for instance it can be used to block the handler to manage the scroll of a view on some scenarios
     * @code
     * // This handler handles automatically the scroll of
     * // flickableItem, unless Ctrl is pressed, in this case the
     * // app has custom code to handle Ctrl+wheel zooming
     * Kirigami.WheelHandler {
     *   target: flickableItem
     *   blockTargetWheel: true
     *   scrollFlickableTarget: true
     *   onWheel: {
     *        if (wheel.modifiers & Qt.ControlModifier) {
     *            wheel.accepted = true;
     *            // Handle scaling of the view
     *       }
     *   }
     * }
     * @endcode
     *
     */
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)

public:
    KirigamiWheelEvent(QObject *parent = nullptr);
    ~KirigamiWheelEvent() override;

    void initializeFromEvent(QWheelEvent *event);

    qreal x() const;
    qreal y() const;
    QPointF angleDelta() const;
    QPointF pixelDelta() const;
    int buttons() const;
    int modifiers() const;
    bool inverted() const;
    bool isAccepted();
    void setAccepted(bool accepted);

private:
    qreal m_x = 0;
    qreal m_y = 0;
    QPointF m_angleDelta;
    QPointF m_pixelDelta;
    Qt::MouseButtons m_buttons = Qt::NoButton;
    Qt::KeyboardModifiers m_modifiers = Qt::NoModifier;
    bool m_inverted = false;
    bool m_accepted = false;
};

class GlobalWheelFilter : public QObject
{
    Q_OBJECT

public:
    GlobalWheelFilter(QObject *parent = nullptr);
    ~GlobalWheelFilter() override;

    static GlobalWheelFilter *self();

    void setItemHandlerAssociation(QQuickItem *item, WheelHandler *handler);
    void removeItemHandlerAssociation(QQuickItem *item, WheelHandler *handler);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void manageWheel(QQuickItem *target, QWheelEvent *wheel);

    QMultiHash<QQuickItem *, WheelHandler *> m_handlersForItem;
    KirigamiWheelEvent m_wheelEvent;
};



/**
 * This class intercepts the mouse wheel events of its target, and gives them to the user code as a signal, which can be used for custom mouse wheel management code.
 * The handler can block completely the wheel events from its target, and if it's a Flickable, it can automatically handle scrolling on it
 */
class WheelHandler : public QObject
{
    Q_OBJECT

    /**
     * target: Item
     *
     * The target we want to manage wheel events.
     * We will receive wheel() signals every time the user moves
     * the mouse wheel (or scrolls with the touchpad) on top
     * of that item.
     */
    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget NOTIFY targetChanged)

    /**
     * blockTargetWheel: bool
     *
     * If true, the target won't receive any wheel event at all (default true)
     */
    Q_PROPERTY(bool blockTargetWheel MEMBER m_blockTargetWheel NOTIFY blockTargetWheelChanged)

    /**
     * scrollFlickableTarget: bool
     * If this property is true and the target is a Flickable, wheel events will cause the Flickable to scroll (default true)
     */
    Q_PROPERTY(bool scrollFlickableTarget MEMBER m_scrollFlickableTarget NOTIFY scrollFlickableTargetChanged)

public:
    explicit WheelHandler(QObject *parent = nullptr);
    ~WheelHandler() override;

    QQuickItem *target() const;
    void setTarget(QQuickItem *target);

Q_SIGNALS:
    void targetChanged();
    void blockTargetWheelChanged();
    void scrollFlickableTargetChanged();
    void wheel(KirigamiWheelEvent *wheel);

private:
    QPointer<QQuickItem> m_target;
    bool m_blockTargetWheel = true;
    bool m_scrollFlickableTarget = true;
    KirigamiWheelEvent m_wheelEvent;

    friend class GlobalWheelFilter;
};



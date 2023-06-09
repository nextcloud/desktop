/*
 *  SPDX-FileCopyrightText: 2019 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "wheelhandler.h"
#include <QWheelEvent>
#include <QQuickWindow>

KirigamiWheelEvent::KirigamiWheelEvent(QObject *parent)
    : QObject(parent)
{
}

KirigamiWheelEvent::~KirigamiWheelEvent()
{
}

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

WheelFilterItem::WheelFilterItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setEnabled(false);
}

///////////////////////////////

WheelHandler::WheelHandler(QObject *parent)
    : QObject(parent)
    , m_filterItem(new WheelFilterItem(nullptr))
{
    m_filterItem->installEventFilter(this);

    m_wheelScrollingTimer.setSingleShot(true);
    m_wheelScrollingTimer.setInterval(m_wheelScrollingDuration);
    m_wheelScrollingTimer.callOnTimeout([this]() {
        setScrolling(false);
    });

    connect(QGuiApplication::styleHints(), &QStyleHints::wheelScrollLinesChanged, this, [this](int scrollLines) {
        m_defaultPixelStepSize = 20 * scrollLines;
        if (!m_explicitVStepSize && m_verticalStepSize != m_defaultPixelStepSize) {
            m_verticalStepSize = m_defaultPixelStepSize;
            Q_EMIT verticalStepSizeChanged();
        }
        if (!m_explicitHStepSize && m_horizontalStepSize != m_defaultPixelStepSize) {
            m_horizontalStepSize = m_defaultPixelStepSize;
            Q_EMIT horizontalStepSizeChanged();
        }
    });
}

WheelHandler::~WheelHandler() = default;

QQuickItem *WheelHandler::target() const
{
    return m_flickable;
}

void WheelHandler::setTarget(QQuickItem *target)
{
    if (m_flickable == target) {
        return;
    }

    if (target && !target->inherits("QQuickFlickable")) {
        qmlWarning(this) << "target must be a QQuickFlickable";
        return;
    }

    if (m_flickable) {
        m_flickable->removeEventFilter(this);
        disconnect(m_flickable, nullptr, m_filterItem, nullptr);
    }

    m_flickable = target;
    m_filterItem->setParentItem(target);

    QQuickItem *vscrollbar = nullptr;
    QQuickItem *hscrollbar = nullptr;

    if (target) {
        target->installEventFilter(this);

        // Stack WheelFilterItem over the Flickable's scrollable content
        m_filterItem->stackAfter(target->property("contentItem").value<QQuickItem*>());
        // Make it fill the Flickable
        m_filterItem->setWidth(target->width());
        m_filterItem->setHeight(target->height());
        connect(target, &QQuickItem::widthChanged, m_filterItem, [this, target](){
            m_filterItem->setWidth(target->width());
        });
        connect(target, &QQuickItem::heightChanged, m_filterItem, [this, target](){
            m_filterItem->setHeight(target->height());
        });

        // Get ScrollBars so that we can filter them too, even if they're not in the bounds of the Flickable
        auto targetChildren = target->children();
        for (auto child : targetChildren) {
            if (child->inherits("QQuickScrollBarAttached")) {
                vscrollbar = child->property("vertical").value<QQuickItem*>();
                hscrollbar = child->property("horizontal").value<QQuickItem*>();
                break;
            }
        }
        // Check ScrollView if there are no scrollbars attached to the Flickable.
        // We need to check if the parent inherits QQuickScrollView in case the
        // parent is another Flickable that already has a Kirigami WheelHandler.
        auto targetParent = target->parentItem();
        if (targetParent && targetParent->inherits("QQuickScrollView") && !vscrollbar && !hscrollbar) {
            auto targetParentChildren = targetParent->children();
            for (auto child : targetParentChildren) {
                if (child->inherits("QQuickScrollBarAttached")) {
                    vscrollbar = child->property("vertical").value<QQuickItem*>();
                    hscrollbar = child->property("horizontal").value<QQuickItem*>();
                    break;
                }
            }
        }
    }

    if (m_verticalScrollBar != vscrollbar) {
        if (m_verticalScrollBar) {
            m_verticalScrollBar->removeEventFilter(this);
        }
        m_verticalScrollBar = vscrollbar;
        if (vscrollbar) {
            vscrollbar->installEventFilter(this);
        }
    }

    if (m_horizontalScrollBar != hscrollbar) {
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->removeEventFilter(this);
        }
        m_horizontalScrollBar = hscrollbar;
        if (hscrollbar) {
            hscrollbar->installEventFilter(this);
        }
    }

    Q_EMIT targetChanged();
}

qreal WheelHandler::verticalStepSize() const
{
    return m_verticalStepSize;
}

void WheelHandler::setVerticalStepSize(qreal stepSize)
{
    m_explicitVStepSize = true;
    if (qFuzzyCompare(m_verticalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetVerticalStepSize();
        return;
    }
    m_verticalStepSize = stepSize;
    Q_EMIT verticalStepSizeChanged();
}

void WheelHandler::resetVerticalStepSize()
{
    m_explicitVStepSize = false;
    if (qFuzzyCompare(m_verticalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_verticalStepSize = m_defaultPixelStepSize;
    Q_EMIT verticalStepSizeChanged();
}

qreal WheelHandler::horizontalStepSize() const
{
    return m_horizontalStepSize;
}

void WheelHandler::setHorizontalStepSize(qreal stepSize)
{
    m_explicitHStepSize = true;
    if (qFuzzyCompare(m_horizontalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetHorizontalStepSize();
        return;
    }
    m_horizontalStepSize = stepSize;
    Q_EMIT horizontalStepSizeChanged();
}

void WheelHandler::resetHorizontalStepSize()
{
    m_explicitHStepSize = false;
    if (qFuzzyCompare(m_horizontalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_horizontalStepSize = m_defaultPixelStepSize;
    Q_EMIT horizontalStepSizeChanged();
}

Qt::KeyboardModifiers WheelHandler::pageScrollModifiers() const
{
    return m_pageScrollModifiers;
}

void WheelHandler::setPageScrollModifiers(Qt::KeyboardModifiers modifiers)
{
    if (m_pageScrollModifiers == modifiers) {
        return;
    }
    m_pageScrollModifiers = modifiers;
    Q_EMIT pageScrollModifiersChanged();
}

void WheelHandler::resetPageScrollModifiers()
{
    setPageScrollModifiers(m_defaultPageScrollModifiers);
}

bool WheelHandler::filterMouseEvents() const
{
    return m_filterMouseEvents;
}

void WheelHandler::setFilterMouseEvents(bool enabled)
{
    if (m_filterMouseEvents == enabled) {
        return;
    }
    m_filterMouseEvents = enabled;
    Q_EMIT filterMouseEventsChanged();
}

bool WheelHandler::keyNavigationEnabled() const
{
    return m_keyNavigationEnabled;
}

void WheelHandler::setKeyNavigationEnabled(bool enabled)
{
    if (m_keyNavigationEnabled == enabled) {
        return;
    }
    m_keyNavigationEnabled = enabled;
    Q_EMIT keyNavigationEnabledChanged();
}

void WheelHandler::setScrolling(bool scrolling)
{
    if (m_wheelScrolling == scrolling) {
        if (m_wheelScrolling) {
            m_wheelScrollingTimer.start();
        }
        return;
    }
    m_wheelScrolling = scrolling;
    m_filterItem->setEnabled(m_wheelScrolling);
}

bool WheelHandler::scrollFlickable(QPointF pixelDelta, QPointF angleDelta, Qt::KeyboardModifiers modifiers)
{
    if (!m_flickable || (pixelDelta.isNull() && angleDelta.isNull())) {
        return false;
    }

    const qreal width = m_flickable->width();
    const qreal height = m_flickable->height();
    const qreal contentWidth = m_flickable->property("contentWidth").toReal();
    const qreal contentHeight = m_flickable->property("contentHeight").toReal();
    const qreal contentX = m_flickable->property("contentX").toReal();
    const qreal contentY = m_flickable->property("contentY").toReal();
    const qreal topMargin = m_flickable->property("topMargin").toReal();
    const qreal bottomMargin = m_flickable->property("bottomMargin").toReal();
    const qreal leftMargin = m_flickable->property("leftMargin").toReal();
    const qreal rightMargin = m_flickable->property("rightMargin").toReal();
    const qreal originX = m_flickable->property("originX").toReal();
    const qreal originY = m_flickable->property("originY").toReal();
    const qreal pageWidth = width - leftMargin - rightMargin;
    const qreal pageHeight = height - topMargin - bottomMargin;
    const auto window = m_flickable->window();
    const qreal devicePixelRatio = window != nullptr ? window->devicePixelRatio() : qGuiApp->devicePixelRatio();

    // HACK: Only transpose deltas when not using xcb in order to not conflict with xcb's own delta transposing
    if (modifiers & m_defaultHorizontalScrollModifiers && qGuiApp->platformName() != QLatin1String("xcb")) {
        angleDelta = angleDelta.transposed();
        pixelDelta = pixelDelta.transposed();
    }

    const qreal xTicks = angleDelta.x() / 120;
    const qreal yTicks = angleDelta.y() / 120;
    qreal xChange = NAN;
    qreal yChange = NAN;
    bool scrolled = false;

    // Scroll X
    if (contentWidth > pageWidth) {
        // Use page size with pageScrollModifiers. Matches QScrollBar, which uses QAbstractSlider behavior.
        if (modifiers & m_pageScrollModifiers) {
            xChange = qBound(-pageWidth, xTicks * pageWidth, pageWidth);
        } else if (pixelDelta.x() != 0) {
            xChange = pixelDelta.x();
        } else {
            xChange = xTicks * m_horizontalStepSize;
        }

        // contentX and contentY use reversed signs from what x and y would normally use, so flip the signs

        qreal minXExtent = leftMargin - originX;
        qreal maxXExtent = width - (contentWidth + rightMargin + originX);

        qreal newContentX = qBound(-minXExtent, contentX - xChange, -maxXExtent);
        // Flickable::pixelAligned rounds the position, so round to mimic that behavior.
        // Rounding prevents fractional positioning from causing text to be
        // clipped off on the top and bottom.
        // Multiply by devicePixelRatio before rounding and divide by devicePixelRatio
        // after to make position match pixels on the screen more closely.
        newContentX = std::round(newContentX * devicePixelRatio) / devicePixelRatio;
        if (contentX != newContentX) {
            scrolled = true;
            m_flickable->setProperty("contentX", newContentX);
        }
    }

    // Scroll Y
    if (contentHeight > pageHeight) {
        if (modifiers & m_pageScrollModifiers) {
            yChange = qBound(-pageHeight, yTicks * pageHeight, pageHeight);
        } else if (pixelDelta.y() != 0) {
            yChange = pixelDelta.y();
        } else {
            yChange = yTicks * m_verticalStepSize;
        }

        // contentX and contentY use reversed signs from what x and y would normally use, so flip the signs

        qreal minYExtent = topMargin - originY;
        qreal maxYExtent = height - (contentHeight + bottomMargin + originY);

        qreal newContentY = qBound(-minYExtent, contentY - yChange, -maxYExtent);
        // Flickable::pixelAligned rounds the position, so round to mimic that behavior.
        // Rounding prevents fractional positioning from causing text to be
        // clipped off on the top and bottom.
        // Multiply by devicePixelRatio before rounding and divide by devicePixelRatio
        // after to make position match pixels on the screen more closely.
        newContentY = std::round(newContentY * devicePixelRatio) / devicePixelRatio;
        if (contentY != newContentY) {
            scrolled = true;
            m_flickable->setProperty("contentY", newContentY);
        }
    }

    return scrolled;
}

bool WheelHandler::scrollUp(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, stepSize));
}

bool WheelHandler::scrollDown(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, -stepSize));
}

bool WheelHandler::scrollLeft(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(stepSize, 0));
}

bool WheelHandler::scrollRight(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(-stepSize, 0));
}

bool WheelHandler::eventFilter(QObject *watched, QEvent *event)
{
    auto item = qobject_cast<QQuickItem*>(watched);
    if (!item || !item->isEnabled()) {
        return false;
    }

    qreal contentWidth = 0;
    qreal contentHeight = 0;
    qreal pageWidth = 0;
    qreal pageHeight = 0;
    if (m_flickable) {
        contentWidth = m_flickable->property("contentWidth").toReal();
        contentHeight = m_flickable->property("contentHeight").toReal();
        pageWidth = m_flickable->width() - m_flickable->property("leftMargin").toReal() - m_flickable->property("rightMargin").toReal();
        pageHeight = m_flickable->height() - m_flickable->property("topMargin").toReal() - m_flickable->property("bottomMargin").toReal();
    }

    // The code handling touch, mouse and hover events is mostly copied/adapted from QQuickScrollView::childMouseEventFilter()
    switch (event->type()) {
    case QEvent::Wheel: {
        // QQuickScrollBar::interactive handling Matches behavior in QQuickScrollView::eventFilter()
        if (m_filterMouseEvents) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        auto *wheelEvent = dynamic_cast<QWheelEvent *>(event);

        // NOTE: On X11 with libinput, pixelDelta is identical to angleDelta when using a mouse that shouldn't use pixelDelta.
        // If faulty pixelDelta, reset pixelDelta to (0,0).
        if (wheelEvent->pixelDelta() == wheelEvent->angleDelta()) {
            // In order to change any of the data, we have to create a whole new QWheelEvent from its constructor.
            QWheelEvent newWheelEvent(
                wheelEvent->position(),
                wheelEvent->globalPosition(),
                QPoint(0,0), // pixelDelta
                wheelEvent->angleDelta(),
                wheelEvent->buttons(),
                wheelEvent->modifiers(),
                wheelEvent->phase(),
                wheelEvent->inverted(),
                wheelEvent->source()
            );
            m_kirigamiWheelEvent.initializeFromEvent(&newWheelEvent);
        } else {
            m_kirigamiWheelEvent.initializeFromEvent(wheelEvent);
        }

        Q_EMIT wheel(&m_kirigamiWheelEvent);

        if (m_kirigamiWheelEvent.isAccepted()) {
            return true;
        }

        bool scrolled = false;
        if (m_scrollFlickableTarget || (contentHeight <= pageHeight && contentWidth <= pageWidth)) {
            // Don't use pixelDelta from the event unless angleDelta is not available
            // because scrolling by pixelDelta is too slow on Wayland with libinput.
            QPointF pixelDelta = m_kirigamiWheelEvent.angleDelta().isNull() ? m_kirigamiWheelEvent.pixelDelta() : QPoint(0, 0);
            scrolled = scrollFlickable(pixelDelta,
                                       m_kirigamiWheelEvent.angleDelta(),
                                       Qt::KeyboardModifiers(m_kirigamiWheelEvent.modifiers()));
        }
        setScrolling(scrolled);

        // NOTE: Wheel events created by touchpad gestures with pixel deltas will cause scrolling to jump back
        // to where scrolling started unless the event is always accepted before it reaches the Flickable.
        bool flickableWillUseGestureScrolling = !(wheelEvent->source() == Qt::MouseEventNotSynthesized || wheelEvent->pixelDelta().isNull());
        return scrolled || m_blockTargetWheel || flickableWillUseGestureScrolling;
    }

    case QEvent::TouchBegin: {
        m_wasTouched = true;
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_verticalScrollBar) {
            m_verticalScrollBar->setProperty("interactive", false);
        }
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->setProperty("interactive", false);
        }
        break;
    }

    case QEvent::TouchEnd: {
        m_wasTouched = false;
        break;
    }

    case QEvent::MouseButtonPress: {
        // NOTE: Flickable does not handle touch events, only synthesized mouse events
        m_wasTouched = dynamic_cast<QMouseEvent *>(event)->source() != Qt::MouseEventNotSynthesized;
        if (!m_filterMouseEvents) {
            break;
        }
        if (!m_wasTouched) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
            break;
        }
        return !m_wasTouched && item == m_flickable;
    }

    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease: {
        setScrolling(false);
        if (!m_filterMouseEvents) {
            break;
        }
        if (dynamic_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized && item == m_flickable) {
            return true;
        }
        break;
    }

    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_wasTouched && (item == m_verticalScrollBar || item == m_horizontalScrollBar)) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        break;
    }

    case QEvent::KeyPress: {
        if (!m_keyNavigationEnabled) {
            break;
        }
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        bool horizontalScroll = keyEvent->modifiers() & m_defaultHorizontalScrollModifiers;
        switch (keyEvent->key()) {
        case Qt::Key_Up: return scrollUp();
        case Qt::Key_Down: return scrollDown();
        case Qt::Key_Left: return scrollLeft();
        case Qt::Key_Right: return scrollRight();
        case Qt::Key_PageUp: return horizontalScroll ? scrollLeft(pageWidth) : scrollUp(pageHeight);
        case Qt::Key_PageDown: return horizontalScroll ? scrollRight(pageWidth) : scrollDown(pageHeight);
        case Qt::Key_Home: return horizontalScroll ? scrollLeft(contentWidth) : scrollUp(contentHeight);
        case Qt::Key_End: return horizontalScroll ? scrollRight(contentWidth) : scrollDown(contentHeight);
        default: break;
        }
        break;
    }

    default: break;
    }

    return false;
}

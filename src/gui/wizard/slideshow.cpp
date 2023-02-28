/*
 * Copyright (C) 2018 by J-P Nurmi <jpnurmi@gmail.com>
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

#include "slideshow.h"
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleHints>

namespace OCC {

static const int Spacing = 6;
static const int SlideDuration = 1000;
static const int SlideDistance = 400;

SlideShow::SlideShow(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setStyleSheet(QStringLiteral("font: bold 18pt"));
}

void SlideShow::addSlide(const QPixmap &pixmap, const QString &label)
{
    _labels += label;
    _pixmaps += pixmap;
    updateGeometry();
}

bool SlideShow::isActive() const
{
    return _timer.isActive();
}

int SlideShow::interval() const
{
    return _interval;
}

void SlideShow::setInterval(int interval)
{
    if (_interval == interval)
        return;

    _interval = interval;
    maybeRestartTimer();

    emit intervalChanged();
}

int SlideShow::currentSlide() const
{
    return _currentIndex;
}

void SlideShow::setCurrentSlide(int index)
{
    if (_currentIndex == index)
        return;

    if (!_animation) {
        _animation = new QVariantAnimation(this);
        _animation->setDuration(SlideDuration);
        _animation->setEasingCurve(QEasingCurve::OutCubic);
        _animation->setStartValue(static_cast<qreal>(_currentIndex));
        connect(_animation.data(), &QVariantAnimation::valueChanged, this, qOverload<>(&SlideShow::update));
    }
    _animation->setEndValue(static_cast<qreal>(index));
    _animation->start(QAbstractAnimation::DeleteWhenStopped);

    _reverse = index < _currentIndex;
    _currentIndex = index;
    maybeRestartTimer();
    update();
    emit currentSlideChanged(index);
}

QSize SlideShow::sizeHint() const
{
    const auto fm = fontMetrics();
    QSize labelSize;
    for (const auto &label : _labels) {
        const auto labelBoundingRect = fm.boundingRect(rect(), Qt::TextWordWrap, label);
        labelSize.setWidth(std::max(labelBoundingRect.width(), labelSize.width()));
        labelSize.setHeight(std::max(labelBoundingRect.height(), labelSize.height()));
    }

    QSize pixmapSize;
    for (const auto &pixmap : _pixmaps) {
        pixmapSize.setWidth(std::max(pixmap.width(), pixmapSize.width()));
        pixmapSize.setHeight(std::max(pixmap.height(), pixmapSize.height()));
    }

    return {
        std::max(labelSize.width(), pixmapSize.width()),
        labelSize.height() + Spacing + pixmapSize.height()
    };
}

void SlideShow::startShow(int interval)
{
    if (interval > 0)
        _interval = interval;
    _timer.start(_interval, this);
}

void SlideShow::stopShow()
{
    _timer.stop();
}

void SlideShow::nextSlide()
{
    setCurrentSlide((_currentIndex + 1) % _labels.count());
    _reverse = false;
}

void SlideShow::prevSlide()
{
    setCurrentSlide((_currentIndex > 0 ? _currentIndex : _labels.count()) - 1);
    _reverse = true;
}

void SlideShow::reset()
{
    stopShow();
    _pixmaps.clear();
    _labels.clear();
    updateGeometry();
    update();
}

void SlideShow::mousePressEvent(QMouseEvent *event)
{
    _pressPoint = event->pos();
}

void SlideShow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!_animation && QLineF(_pressPoint, event->pos()).length() < QGuiApplication::styleHints()->startDragDistance())
        emit clicked();
}

void SlideShow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (_animation) {
        int from = _animation->startValue().toInt();
        int to = _animation->endValue().toInt();
        qreal progress = _animation->easingCurve().valueForProgress(_animation->currentTime() / static_cast<qreal>(_animation->duration()));

        painter.save();
        painter.setOpacity(1.0 - progress);
        painter.translate(progress * (_reverse ? SlideDistance : -SlideDistance), 0);
        drawSlide(&painter, from);

        painter.restore();
        painter.setOpacity(progress);
        painter.translate((1.0 - progress) * (_reverse ? -SlideDistance : SlideDistance), 0);
        drawSlide(&painter, to);
    } else {
        drawSlide(&painter, _currentIndex);
    }
}

void SlideShow::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == _timer.timerId())
        nextSlide();
}

void SlideShow::maybeRestartTimer()
{
    if (!isActive())
        return;

    startShow();
}

void SlideShow::drawSlide(QPainter *painter, int index)
{    
    const auto label = _labels.value(index);
    const auto labelRect = style()->itemTextRect(fontMetrics(),
                                                 rect(),
                                                 Qt::AlignBottom | Qt::AlignHCenter | Qt::TextWordWrap,
                                                 isEnabled(),
                                                 label);
    style()->drawItemText(painter,
                          labelRect,
                          Qt::AlignCenter | Qt::TextWordWrap,
                          palette(),
                          isEnabled(),
                          label,
                          QPalette::WindowText);

    const auto pixmap = _pixmaps.value(index);
    const auto pixmapRect = style()->itemPixmapRect(QRect(0, 0, width(), labelRect.top() - Spacing),
                                                    Qt::AlignCenter,
                                                    pixmap);
    style()->drawItemPixmap(painter,
                            pixmapRect,
                            Qt::AlignCenter,
                            pixmap);
}

} // namespace OCC

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

#ifndef OCC_SLIDESHOW_H
#define OCC_SLIDESHOW_H

#include <QWidget>
#include <QBasicTimer>
#include <QPointer>
#include <QVariantAnimation>

namespace OCC {

/**
 * @brief The SlideShow class
 * @ingroup gui
 */
class SlideShow : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int interval READ interval WRITE setInterval)
    Q_PROPERTY(int currentSlide READ currentSlide WRITE setCurrentSlide NOTIFY currentSlideChanged)

public:
    explicit SlideShow(QWidget* parent = nullptr);

    void addSlide(const QPixmap &pixmap, const QString &label);

    bool isActive() const;

    int interval() const;
    void setInterval(int interval);

    int currentSlide() const;
    void setCurrentSlide(int index);

    QSize sizeHint() const override;

public slots:
    void startShow(int interval = 0);
    void stopShow();
    void nextSlide();
    void previousSlide();
    void reset();

signals:
    void clicked();
    void currentSlideChanged(int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void maybeRestartTimer();
    void drawSlide(QPainter *painter, int index);

    bool _reverse = false;
    int _interval = 2500;
    int _currentIndex = 0;
    QPoint _pressPoint;
    QBasicTimer _timer;
    QStringList _labels;
    QVector<QPixmap> _pixmaps;
    QPointer<QVariantAnimation> _animation = nullptr;
};

} // namespace OCC

#endif // OCC_SLIDESHOW_H

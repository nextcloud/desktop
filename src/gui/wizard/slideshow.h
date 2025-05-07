/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(int currentSlide READ currentSlide WRITE setCurrentSlide NOTIFY currentSlideChanged)

public:
    explicit SlideShow(QWidget* parent = nullptr);

    void addSlide(const QPixmap &pixmap, const QString &label);

    [[nodiscard]] bool isActive() const;

    [[nodiscard]] int interval() const;
    void setInterval(int interval);

    [[nodiscard]] int currentSlide() const;
    void setCurrentSlide(int index);

    [[nodiscard]] QSize sizeHint() const override;

public slots:
    void startShow(int interval = 0);
    void stopShow();
    void nextSlide();
    void prevSlide();
    void reset();

signals:
    void clicked();
    void currentSlideChanged(int index);
    void intervalChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void maybeRestartTimer();
    void drawSlide(QPainter *painter, int index);

    bool _reverse = false;
    int _interval = 3500;
    int _currentIndex = 0;
    QPoint _pressPoint;
    QBasicTimer _timer;
    QStringList _labels;
    QVector<QPixmap> _pixmaps;
    QPointer<QVariantAnimation> _animation = nullptr;
};

} // namespace OCC

#endif // OCC_SLIDESHOW_H

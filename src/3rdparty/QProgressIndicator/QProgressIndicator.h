/*
*
* This file is part of QProgressIndicator,
* an open-source recent files menu widget
*
* Copyright (C) 2009 - 2010 Morgan Leborgne
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QRecentFilesMenu. If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifndef QPROGRESSINDICATOR_H
#define QPROGRESSINDICATOR_H

#include <QWidget>
#include <QColor>

/*! 
    \class QProgressIndicator
    \brief The QProgressIndicator class lets an application display a progress indicator to show that a lengthy task is under way. 

    Progress indicators are indeterminate and do nothing more than spin to show that the application is busy.
    \sa QProgressBar
*/
class QProgressIndicator : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int delay READ animationDelay WRITE setAnimationDelay)
    Q_PROPERTY(bool displayedWhenStopped READ isDisplayedWhenStopped WRITE setDisplayedWhenStopped)
    Q_PROPERTY(QColor color READ color WRITE setColor)
public:
    QProgressIndicator(QWidget* parent = 0);

    /*! Returns the delay between animation steps.
        \return The number of milliseconds between animation steps. By default, the animation delay is set to 40 milliseconds.
        \sa setAnimationDelay
     */
    int animationDelay() const { return m_delay; }

    /*! Returns a Boolean value indicating whether the component is currently animated.
        \return Animation state.
        \sa startAnimation stopAnimation
     */
    bool isAnimated () const;

    /*! Returns a Boolean value indicating whether the receiver shows itself even when it is not animating.
        \return Return true if the progress indicator shows itself even when it is not animating. By default, it returns false.
        \sa setDisplayedWhenStopped
     */
    bool isDisplayedWhenStopped() const;

    /*! Returns the color of the component.
        \sa setColor
      */
    const QColor & color() const { return m_color; }

    virtual QSize sizeHint() const Q_DECL_OVERRIDE;
    int heightForWidth(int w) const Q_DECL_OVERRIDE;
public slots:
    /*! Starts the spin animation.
        \sa stopAnimation isAnimated
     */
    void startAnimation();

    /*! Stops the spin animation.
        \sa startAnimation isAnimated
     */
    void stopAnimation();

    /*! Sets the delay between animation steps.
        Setting the \a delay to a value larger than 40 slows the animation, while setting the \a delay to a smaller value speeds it up.
        \param delay The delay, in milliseconds. 
        \sa animationDelay 
     */
    void setAnimationDelay(int delay);

    /*! Sets whether the component hides itself when it is not animating. 
       \param state The animation state. Set false to hide the progress indicator when it is not animating; otherwise true.
       \sa isDisplayedWhenStopped
     */
    void setDisplayedWhenStopped(bool state);

    /*! Sets the color of the components to the given color.
        \sa color
     */
    void setColor(const QColor & color);
protected:
    virtual void timerEvent(QTimerEvent * event) Q_DECL_OVERRIDE; 
    virtual void paintEvent(QPaintEvent * event) Q_DECL_OVERRIDE;
private:
    int m_angle;
    int m_timerId;
    int m_delay;
    bool m_displayedWhenStopped;
    QColor m_color;
};

#endif // QPROGRESSINDICATOR_H

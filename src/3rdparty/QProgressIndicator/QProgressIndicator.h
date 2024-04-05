/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2011 Morgan Leborgne
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    QProgressIndicator(QWidget* parent = nullptr);

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

    QSize sizeHint() const override;
    int heightForWidth(int w) const override;
public Q_SLOTS:
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
    void timerEvent(QTimerEvent * event) override; 
    void paintEvent(QPaintEvent * event) override;
private:
    int m_angle;
    int m_timerId;
    int m_delay;
    bool m_displayedWhenStopped;
    QColor m_color;
};

#endif // QPROGRESSINDICATOR_H

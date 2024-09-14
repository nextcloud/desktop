/* This file is part of the KDE libraries
 *
 * Copyright (c) 2011 Aurélien Gâteau <agateau@kde.org>
 * Copyright (c) 2014 Dominik Haumann <dhaumann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */
#include "kmessagewidget.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QShowEvent>
#include <QTimeLine>
#include <QToolButton>
#include <QStyle>

//---------------------------------------------------------------------
// KMessageWidgetPrivate
//---------------------------------------------------------------------
class KMessageWidgetPrivate
{
public:
    void init(KMessageWidget *);

    KMessageWidget *q = nullptr;
    QFrame *content = nullptr;
    QLabel *iconLabel = nullptr;
    QLabel *textLabel = nullptr;
    QToolButton *closeButton = nullptr;
    QTimeLine *timeLine = nullptr;
    QIcon icon;
    bool ignoreShowEventDoingAnimatedShow = false;

    KMessageWidget::MessageType messageType = KMessageWidget::Positive;
    bool wordWrap = false;
    QList<QToolButton *> buttons;
    QPixmap contentSnapShot;

    void createLayout();
    void applyStyleSheet();
    void updateSnapShot();
    void updateLayout();
    void slotTimeLineChanged(qreal);
    void slotTimeLineFinished();

    [[nodiscard]] int bestContentHeight() const;
};

void KMessageWidgetPrivate::init(KMessageWidget *q_ptr)
{
    q = q_ptr;

    q->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Note: when changing the value 500, also update KMessageWidgetTest
    timeLine = new QTimeLine(500, q);
    QObject::connect(timeLine, &QTimeLine::valueChanged, q, [this] (qreal value) { slotTimeLineChanged(value); });
    QObject::connect(timeLine, &QTimeLine::finished, q, [this] () { slotTimeLineFinished(); });

    content = new QFrame(q);
    content->setObjectName(QStringLiteral("contentWidget"));
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    wordWrap = false;

    iconLabel = new QLabel(content);
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    iconLabel->hide();

    textLabel = new QLabel(content);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QObject::connect(textLabel, &QLabel::linkActivated, q, &KMessageWidget::linkActivated);
    QObject::connect(textLabel, &QLabel::linkHovered, q, &KMessageWidget::linkHovered);

    auto *closeAction = new QAction(q);
    closeAction->setText(KMessageWidget::tr("&Close"));
    closeAction->setToolTip(KMessageWidget::tr("Close message"));
    closeAction->setIcon(QIcon(":/client/theme/close.svg")); // ivan: NC customization

    QObject::connect(closeAction, &QAction::triggered, q, &KMessageWidget::animatedHide);

    closeButton = new QToolButton(content);
    closeButton->setAutoRaise(true);
    closeButton->setDefaultAction(closeAction);

    q->setMessageType(KMessageWidget::Information);
}

void KMessageWidgetPrivate::createLayout()
{
    delete content->layout();

    content->resize(q->size());

    qDeleteAll(buttons);
    buttons.clear();

    Q_FOREACH (QAction *action, q->actions()) {
        auto *button = new QToolButton(content);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        buttons.append(button);
    }

    // AutoRaise reduces visual clutter, but we don't want to turn it on if
    // there are other buttons, otherwise the close button will look different
    // from the others.
    closeButton->setAutoRaise(buttons.isEmpty());

    if (wordWrap) {
        auto *layout = new QGridLayout(content);
        // Set alignment to make sure icon does not move down if text wraps
        layout->addWidget(iconLabel, 0, 0, 1, 1, Qt::AlignHCenter | Qt::AlignTop);
        layout->addWidget(textLabel, 0, 1);

        if (buttons.isEmpty()) {
            // Use top-vertical alignment like the icon does.
            layout->addWidget(closeButton, 0, 2, 1, 1, Qt::AlignHCenter | Qt::AlignTop);
        } else {
            // Use an additional layout in row 1 for the buttons.
            auto *buttonLayout = new QHBoxLayout;
            buttonLayout->addStretch();
            Q_FOREACH (QToolButton *button, buttons) {
                // For some reason, calling show() is necessary if wordwrap is true,
                // otherwise the buttons do not show up. It is not needed if
                // wordwrap is false.
                button->show();
                buttonLayout->addWidget(button);
            }
            buttonLayout->addWidget(closeButton);
            layout->addItem(buttonLayout, 1, 0, 1, 2);
        }
    } else {
        auto *layout = new QHBoxLayout(content);
        layout->addWidget(iconLabel);
        layout->addWidget(textLabel);

        for (QToolButton *button : std::as_const(buttons)) {
            layout->addWidget(button);
        }

        layout->addWidget(closeButton);
    };

    if (q->isVisible()) {
        q->setFixedHeight(content->sizeHint().height());
    }
    q->updateGeometry();
}

void KMessageWidgetPrivate::applyStyleSheet()
{
    QColor bgBaseColor;

    // We have to hardcode colors here because KWidgetsAddons is a tier 1 framework
    // and therefore can't depend on any other KDE Frameworks
    // The following RGB color values come from the "default" scheme in kcolorscheme.cpp
    switch (messageType) {
    case KMessageWidget::Positive:
        bgBaseColor.setRgb(39, 174,  96); // Window: ForegroundPositive
        break;
    case KMessageWidget::Information:
        bgBaseColor.setRgb(61, 174, 233); // Window: ForegroundActive
        break;
    case KMessageWidget::Warning:
        bgBaseColor.setRgb(246, 116, 0); // Window: ForegroundNeutral
        break;
    case KMessageWidget::Error:
        bgBaseColor.setRgb(218, 68, 83); // Window: ForegroundNegative
        break;
    }
    const qreal bgBaseColorAlpha = 0.2;
    bgBaseColor.setAlphaF(bgBaseColorAlpha);

    const QPalette palette = QGuiApplication::palette();
    const QColor windowColor = palette.window().color();
    const QColor textColor = palette.text().color();
    const QColor border = bgBaseColor;

    // Generate a final background color from overlaying bgBaseColor over windowColor
    const int newRed = qRound(bgBaseColor.red() * bgBaseColorAlpha) + qRound(windowColor.red() * (1 - bgBaseColorAlpha));
    const int newGreen = qRound(bgBaseColor.green() * bgBaseColorAlpha) + qRound(windowColor.green() * (1 - bgBaseColorAlpha));
    const int newBlue = qRound(bgBaseColor.blue() * bgBaseColorAlpha) + qRound(windowColor.blue() * (1 - bgBaseColorAlpha));

    const QColor bgFinalColor = QColor(newRed, newGreen, newBlue);

    content->setStyleSheet(
        QString::fromLatin1(".QFrame {"
                              "background-color: %1;"
                              "border-radius: 4px;"
                              "border: 2px solid %2;"
                              "margin: %3px;"
                              "}"
                              ".QLabel { color: %4; }"
                             )
        .arg(bgFinalColor.name())
        .arg(border.name())
        // DefaultFrameWidth returns the size of the external margin + border width. We know our border is 1px, so we subtract this from the frame normal QStyle FrameWidth to get our margin
        .arg(q->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, q) - 1)
        .arg(textColor.name())
    );
}

void KMessageWidgetPrivate::updateLayout()
{
    if (content->layout()) {
        createLayout();
    }
}

void KMessageWidgetPrivate::updateSnapShot()
{
    // Attention: updateSnapShot calls QWidget::render(), which causes the whole
    // window layouts to be activated. Calling this method from resizeEvent()
    // can lead to infinite recursion, see:
    // https://bugs.kde.org/show_bug.cgi?id=311336
    contentSnapShot = QPixmap(content->size() * q->devicePixelRatio());
    contentSnapShot.setDevicePixelRatio(q->devicePixelRatio());
    contentSnapShot.fill(Qt::transparent);
    content->render(&contentSnapShot, QPoint(), QRegion(), QWidget::DrawChildren);
}

void KMessageWidgetPrivate::slotTimeLineChanged(qreal value)
{
    q->setFixedHeight(qMin(qRound(value * 2.0), 1) * content->height());
    q->update();
}

void KMessageWidgetPrivate::slotTimeLineFinished()
{
    if (timeLine->direction() == QTimeLine::Forward) {
        // Show
        // We set the whole geometry here, because it may be wrong if a
        // KMessageWidget is shown right when the toplevel window is created.
        content->setGeometry(0, 0, q->width(), bestContentHeight());

        // notify about finished animation
        emit q->showAnimationFinished();
    } else {
        // hide and notify about finished animation
        q->hide();
        emit q->hideAnimationFinished();
    }
}

int KMessageWidgetPrivate::bestContentHeight() const
{
    int height = content->heightForWidth(q->width());
    if (height == -1) {
        height = content->sizeHint().height();
    }
    return height;
}

//---------------------------------------------------------------------
// KMessageWidget
//---------------------------------------------------------------------
KMessageWidget::KMessageWidget(QWidget *parent)
    : QFrame(parent)
    , d(new KMessageWidgetPrivate)
{
    d->init(this);
}

KMessageWidget::KMessageWidget(const QString &text, QWidget *parent)
    : QFrame(parent)
    , d(new KMessageWidgetPrivate)
{
    d->init(this);
    setText(text);
}

KMessageWidget::~KMessageWidget()
{
    delete d;
}

QString KMessageWidget::text() const
{
    return d->textLabel->text();
}

void KMessageWidget::setText(const QString &text)
{
    d->textLabel->setText(text);
    updateGeometry();
}

KMessageWidget::MessageType KMessageWidget::messageType() const
{
    return d->messageType;
}

void KMessageWidget::setMessageType(KMessageWidget::MessageType type)
{
    d->messageType = type;
    d->applyStyleSheet();
}

QSize KMessageWidget::sizeHint() const
{
    ensurePolished();
    return d->content->sizeHint();
}

QSize KMessageWidget::minimumSizeHint() const
{
    ensurePolished();
    return d->content->minimumSizeHint();
}

bool KMessageWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Polish && !d->content->layout()) {
        d->createLayout();
    } else if (event->type() == QEvent::PaletteChange) {
        d->applyStyleSheet();
    } else if (event->type() == QEvent::Show && !d->ignoreShowEventDoingAnimatedShow) {
        if ((height() != d->content->height()) || (d->content->pos().y() != 0)) {
            d->content->move(0, 0);
            setFixedHeight(d->content->height());
        }
    }
    return QFrame::event(event);
}

void KMessageWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->content->resize(width(), d->bestContentHeight());
    }
}

int KMessageWidget::heightForWidth(int width) const
{
    ensurePolished();
    return d->content->heightForWidth(width);
}

void KMessageWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    if (d->timeLine->state() == QTimeLine::Running) {
        QPainter painter(this);
        painter.setOpacity(d->timeLine->currentValue() * d->timeLine->currentValue());
        painter.drawPixmap(0, 0, d->contentSnapShot);
    }
}

bool KMessageWidget::wordWrap() const
{
    return d->wordWrap;
}

void KMessageWidget::setWordWrap(bool wordWrap)
{
    d->wordWrap = wordWrap;
    d->textLabel->setWordWrap(wordWrap);
    QSizePolicy policy = sizePolicy();
    policy.setHeightForWidth(wordWrap);
    setSizePolicy(policy);
    d->updateLayout();
    // Without this, when user does wordWrap -> !wordWrap -> wordWrap, a minimum
    // height is set, causing the widget to be too high.
    // Mostly visible in test programs.
    if (wordWrap) {
        setMinimumHeight(0);
    }
}

bool KMessageWidget::isCloseButtonVisible() const
{
    return d->closeButton->isVisible();
}

void KMessageWidget::setCloseButtonVisible(bool show)
{
    d->closeButton->setVisible(show);
    updateGeometry();
}

void KMessageWidget::addAction(QAction *action)
{
    QFrame::addAction(action);
    d->updateLayout();
}

void KMessageWidget::removeAction(QAction *action)
{
    QFrame::removeAction(action);
    d->updateLayout();
}

void KMessageWidget::animatedShow()
{
    // Test before styleHint, as there might have been a style change while animation was running
    if (isHideAnimationRunning()) {
        d->timeLine->stop();
        emit hideAnimationFinished();
    }

    if (!style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)
     || (parentWidget() && !parentWidget()->isVisible())) {
        show();
        emit showAnimationFinished();
        return;
    }

    if (isVisible() && (d->timeLine->state() == QTimeLine::NotRunning) && (height() == d->bestContentHeight()) && (d->content->pos().y() == 0)) {
        emit showAnimationFinished();
        return;
    }

    d->ignoreShowEventDoingAnimatedShow = true;
    show();
    d->ignoreShowEventDoingAnimatedShow = false;
    setFixedHeight(0);
    int wantedHeight = d->bestContentHeight();
    d->content->setGeometry(0, -wantedHeight, width(), wantedHeight);

    d->updateSnapShot();

    d->timeLine->setDirection(QTimeLine::Forward);
    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->timeLine->start();
    }
}

void KMessageWidget::animatedHide()
{
    // test this before isVisible, as animatedShow might have been called directly before,
    // so the first timeline event is not yet done and the widget is still hidden
    // And before styleHint, as there might have been a style change while animation was running
    if (isShowAnimationRunning()) {
        d->timeLine->stop();
        emit showAnimationFinished();
    }

    if (!style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) {
        hide();
        emit hideAnimationFinished();
        return;
    }

    if (!isVisible()) {
        // explicitly hide it, so it stays hidden in case it is only not visible due to the parents
        hide();
        emit hideAnimationFinished();
        return;
    }

    d->content->move(0, -d->content->height());
    d->updateSnapShot();

    d->timeLine->setDirection(QTimeLine::Backward);
    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->timeLine->start();
    }
}

bool KMessageWidget::isHideAnimationRunning() const
{
    return (d->timeLine->direction() == QTimeLine::Backward)
        && (d->timeLine->state() == QTimeLine::Running);
}

bool KMessageWidget::isShowAnimationRunning() const
{
    return (d->timeLine->direction() == QTimeLine::Forward)
        && (d->timeLine->state() == QTimeLine::Running);
}

QIcon KMessageWidget::icon() const
{
    return d->icon;
}

void KMessageWidget::setIcon(const QIcon &icon)
{
    d->icon = icon;
    if (d->icon.isNull()) {
        d->iconLabel->hide();
    } else {
        const int size = style()->pixelMetric(QStyle::PM_ToolBarIconSize);
        d->iconLabel->setPixmap(d->icon.pixmap(size));
        d->iconLabel->show();
    }
}

#include "moc_kmessagewidget.cpp"


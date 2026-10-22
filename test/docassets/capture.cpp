/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capture.h"
#include "accountmanager.h"
#include "capturescene.h"
#include "fixtureimageprovider.h"
#include "nativetitlebar_mac.h"
#include "offlinenetworkfactory.h"
#include "theme.h"
#include "tray/svgimageprovider.h"
#include "tray/usermodel.h"
#include "wizardfixtures.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QWidget>
#include <memory>

namespace OCC::DocAssets
{
namespace
{
constexpr auto readinessIntervalMs = 40;
constexpr auto requiredRenderedFrames = 3;

bool imagesReady(QQuickItem *item, QString *error)
{
    constexpr auto imageReady = 1;
    constexpr auto imageError = 3;
    if (item->inherits("QQuickImage") && !item->property("source").toUrl().isEmpty()) {
        const auto status = item->property("status").toInt();
        if (status == imageError) {
            *error = QStringLiteral("Image failed to load: %1").arg(item->property("source").toUrl().toString());
        }
        if (status != imageReady) {
            return false;
        }
    }
    for (auto *child : item->childItems()) {
        if (!imagesReady(child, error)) {
            return false;
        }
    }
    return true;
}

QQuickWindow *popupWindow(QQuickWindow *parent)
{
    for (auto *object : parent->findChildren<QObject *>()) {
        if (!object->inherits("QQuickPopup") || !object->property("opened").toBool()) {
            continue;
        }
        auto *content = object->property("contentItem").value<QQuickItem *>();
        if (content && content->window()) {
            return content->window();
        }
    }
    return nullptr;
}

bool saveImage(const QImage &image, const QString &filename, QString *error)
{
    if (image.isNull() || !image.save(filename, "PNG") || QImageReader(filename, "PNG").read().isNull()) {
        *error = QStringLiteral("Could not save and read back %1").arg(filename);
        return false;
    }
    return true;
}
}

void recordQmlWarnings(const QList<QQmlError> &messages, QString *error)
{
    for (const auto &message : messages) {
        const auto description = message.description();
        const auto nativeBackgroundWarning = message.messageType() == QtWarningMsg
            && (message.url() == QUrl(QStringLiteral("qrc:/qml/src/gui/wizard/qml/WizardTextField.qml"))
                || message.url() == QUrl(QStringLiteral("qrc:/qml/src/gui/EmojiPicker.qml")))
            && description.contains(QStringLiteral("The current style does not support customization of this control"))
            && description.contains(QStringLiteral("(property: \"background\" item:"));
        if (!nativeBackgroundWarning && error->isEmpty()) {
            *error = message.toString();
        }
    }
}

bool captureScenario(const QString &scenario, const QString &stagingDirectory, int timeoutMs, QString *error, const QUrl &sourceOverride)
{
    error->clear();
    const auto filename = captureFilename(scenario);
    if (filename.isEmpty()) {
        *error = QStringLiteral("Unknown or unsupported scenario: %1").arg(scenario);
        return false;
    }
    if (!AccountManager::instance()->accounts().isEmpty() || UserModel::instance()->currentUser()) {
        *error = QStringLiteral("Capture requires an empty account model");
        return false;
    }
    CaptureScene scene;
    const auto prepared = scenario.startsWith(QStringLiteral("wizard-")) && prepareWizard(scene, scenario, error);
    if (!prepared) {
        if (error->isEmpty()) {
            *error = QStringLiteral("Scenario requires the dedicated live-account worker: %1").arg(scenario);
        }
        return false;
    }
    OfflineNetworkFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setUiLanguage(QStringLiteral("en"));
    engine.setNetworkAccessManagerFactory(&networkFactory);
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    engine.addImportPath(QStringLiteral("qrc:/qml/theme"));
    engine.addImageProvider(QStringLiteral("svgimage-custom-color"), new Ui::SvgImageProvider);
    auto *images = new FixtureImageProvider;
    engine.addImageProvider(QStringLiteral("tray-image-provider"), images);
    QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
        recordQmlWarnings(warnings, error);
    });
    std::unique_ptr<QObject> root;
    QQuickWindow *window = nullptr;
    if (!scene.widget) {
        QQmlComponent component(&engine);
        if (!sourceOverride.isEmpty()) {
            component.loadUrl(sourceOverride);
        } else if (!scene.module.isEmpty()) {
            component.loadFromModule(scene.module, scene.type);
        } else {
            component.loadUrl(scene.source);
        }
        root.reset(component.createWithInitialProperties(scene.properties));
        window = qobject_cast<QQuickWindow *>(root.get());
        if (!window || component.isError()) {
            *error = QStringLiteral("Could not create production window: %1").arg(component.errorString());
            return false;
        }
    }
    QEventLoop loop;
    auto renderedFrames = 0;
    auto readyFrame = -1;
    auto stableWidgetTicks = 0;
    auto captured = false;
    auto activated = false;
    auto readySize = QSize{};
    QQuickWindow *target = nullptr;
    QMetaObject::Connection frameConnection;
    if (window) {
        window->setIcon(Theme::instance()->applicationIcon());
        window->show();
        window->raise();
        window->requestActivate();
        styleNativeTitleBar(window, true);
    } else {
        scene.widget->show();
    }
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        *error = QStringLiteral("Timed out waiting for controller state, popup, images, layout, or rendering");
        loop.quit();
    });
    deadline.start(qMax(0, timeoutMs));
    QTimer readiness;
    QObject::connect(&readiness, &QTimer::timeout, &loop, [&] {
        if ((window && !window->isVisible()) || (scene.widget && !scene.widget->isVisible())) {
            *error = QStringLiteral("Capture cancelled");
            loop.quit();
            return;
        }
        if (networkFactory.requestAttempted() || images->rejectedRequest()) {
            *error = QStringLiteral("Unexpected network request from production QML");
        }
        const auto controllerReady = scene.ready(error);
        if (!error->isEmpty()) {
            loop.quit();
            return;
        }
        if (!controllerReady) {
            readyFrame = -1;
            return;
        }
        if (!activated) {
            activated = scene.activate(window, error);
            return;
        }
        if (!scene.settled(window, error)) {
            readyFrame = -1;
            return;
        }
        if (scene.widget) {
            const auto size = scene.widget->size();
            if (size != readySize) {
                readySize = size;
                stableWidgetTicks = 0;
            }
            if (++stableWidgetTicks < requiredRenderedFrames) {
                return;
            }
            captured = saveImage(scene.widget->grab().toImage(), QDir(stagingDirectory).filePath(filename), error);
            loop.quit();
            return;
        }
        auto *nextTarget = scene.expectsPopup ? popupWindow(window) : window;
        if (!nextTarget) {
            readyFrame = -1;
            return;
        }
        if (target != nextTarget) {
            QObject::disconnect(frameConnection);
            target = nextTarget;
            renderedFrames = 0;
            readyFrame = -1;
            frameConnection = QObject::connect(
                target,
                &QQuickWindow::frameSwapped,
                &loop,
                [&] {
                    ++renderedFrames;
                },
                Qt::QueuedConnection);
        }
        const auto ready = target->isExposed() && target->isSceneGraphInitialized() && imagesReady(target->contentItem(), error);
        target->update();
        if (!ready || readySize != target->size()) {
            readyFrame = -1;
            readySize = target->size();
            return;
        }
        if (readyFrame < 0) {
            readyFrame = renderedFrames;
            return;
        }
        if (renderedFrames - readyFrame < requiredRenderedFrames) {
            return;
        }
        captured = saveImage(target->grabWindow(), QDir(stagingDirectory).filePath(filename), error);
        loop.quit();
    });
    readiness.start(readinessIntervalMs);
    loop.exec();
    if (!AccountManager::instance()->accounts().isEmpty() || UserModel::instance()->currentUser()) {
        *error = QStringLiteral("Unexpected account registration during capture");
    }
    return captured && error->isEmpty();
}

}

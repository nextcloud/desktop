/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lowpowergpu_mac.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QPlatformSurfaceEvent>
#include <QQuickGraphicsDevice>
#include <QQuickWindow>

#import <Metal/Metal.h>

static_assert(__has_feature(objc_arc), "lowpowergpu_mac requires ARC.");

Q_LOGGING_CATEGORY(lcLowPowerGpu, "nextcloud.gui.lowpowergpu", QtInfoMsg)

namespace {

id<MTLDevice> copyLowPowerDevice()
{
    // Enumerating the devices does not switch GPUs; creating the default device would.
    NSArray<id<MTLDevice>> *const devices = MTLCopyAllDevices();
    id<MTLDevice> lowPowerDevice = nil;

    if (devices.count > 1) {
        for (id<MTLDevice> device in devices) {
            if (device.isLowPower && !device.isHeadless && !device.isRemovable) {
                lowPowerDevice = device;
                break;
            }
        }
    }

    return lowPowerDevice;
}

/*
 * Qt Quick creates a window's QRhi, and with it the MTLDevice, when the window is first exposed.
 * The platform surface is created just before that, so it is the last moment to set the device.
 */
class LowPowerGpuFilter : public QObject
{
public:
    LowPowerGpuFilter(id<MTLDevice> device, QObject *parent)
        : QObject(parent)
        , _device(device)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            if (auto *const window = qobject_cast<QQuickWindow *>(watched); window && window->surfaceType() == QSurface::MetalSurface) {
                window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndCommandQueue(static_cast<MTLDevice *>(_device), nullptr));
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    id<MTLDevice> _device;
};

}

namespace OCC {
namespace Mac {

void preferLowPowerGpu(QCoreApplication *app)
{
    id<MTLDevice> const device = copyLowPowerDevice();
    if (!device) {
        return;
    }

    qCInfo(lcLowPowerGpu) << "Rendering Qt Quick windows on the low-power GPU:" << QString::fromNSString(device.name);
    app->installEventFilter(new LowPowerGpuFilter(device, app));
}

} // namespace Mac
} // namespace OCC

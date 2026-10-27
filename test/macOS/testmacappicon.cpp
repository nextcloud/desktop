/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "libsync/theme_mac.h"

#include <QtTest>

#include <array>
#include <ranges>
#include <tuple>
#include <vector>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

class TestMacAppIcon : public QObject
{
    Q_OBJECT

private:
    static constexpr auto iconSides = std::array{16, 32, 64, 128, 256, 512, 1024};
    static constexpr auto sourceImageSide = 32;

    bool installHook(Method method, id implementationBlock)
    {
        if (!method) {
            return false;
        }

        const auto replacement = imp_implementationWithBlock(implementationBlock);
        if (!replacement) {
            return false;
        }

        _hooks.emplace_back(method, method_getImplementation(method), replacement);
        method_setImplementation(method, replacement);
        return true;
    }

    void verifyReleasedImages() const
    {
        QCOMPARE(_conversionCount, static_cast<int>(iconSides.size()));
        QCOMPARE(static_cast<int>(_resizedImages.allObjects.count), 0);
    }

    std::vector<std::tuple<Method, IMP, IMP>> _hooks;
    NSImage *_sourceImage = nil;
    NSHashTable<NSImage *> *_resizedImages = nil;
    qreal _imageScale = 1.0;
    int _conversionCount = 0;
    bool _missingTiff = false;
    bool _missingPng = false;
    bool _invalidPng = false;

private Q_SLOTS:
    void init()
    {
        [NSApplication sharedApplication];
        _conversionCount = 0;
        _missingTiff = false;
        _missingPng = false;
        _invalidPng = false;
        _resizedImages = [NSHashTable weakObjectsHashTable];
        _sourceImage = [[NSImage alloc] initWithSize:NSMakeSize(sourceImageSide, sourceImageSide)];
        [_sourceImage lockFocus];
        [NSColor.whiteColor setFill];
        NSRectFill(NSMakeRect(0, 0, sourceImageSide, sourceImageSide));
        [_sourceImage unlockFocus];

        // Native image buffers use the display's backing scale.
        const auto sourceBitmap = [NSBitmapImageRep imageRepWithData:_sourceImage.TIFFRepresentation];
        QVERIFY(sourceBitmap);
        _imageScale = static_cast<qreal>(sourceBitmap.pixelsWide) / sourceImageSide;

        const auto imageNamedMethod = class_getClassMethod(NSImage.class, @selector(imageNamed:));
        QVERIFY(imageNamedMethod);
        const auto imageNamedOriginal = reinterpret_cast<NSImage *(*)(id, SEL, NSImageName)>(method_getImplementation(imageNamedMethod));
        QVERIFY(installHook(imageNamedMethod, ^NSImage *(id receiver, NSImageName name) {
            if ([name isEqualToString:@"AppIcon"]) {
                return _sourceImage;
            }
            return imageNamedOriginal(receiver, @selector(imageNamed:), name);
        }));

        const auto tiffMethod = class_getInstanceMethod(NSImage.class, @selector(TIFFRepresentation));
        QVERIFY(tiffMethod);
        const auto tiffOriginal = reinterpret_cast<NSData *(*)(id, SEL)>(method_getImplementation(tiffMethod));
        QVERIFY(installHook(tiffMethod, ^NSData *(NSImage *receiver) {
            ++_conversionCount;
            [_resizedImages addObject:receiver];
            return _missingTiff ? nil : tiffOriginal(receiver, @selector(TIFFRepresentation));
        }));

        const auto pngMethod = class_getInstanceMethod(NSBitmapImageRep.class, @selector(representationUsingType:properties:));
        QVERIFY(pngMethod);
        const auto pngOriginal = reinterpret_cast<NSData *(*)(id, SEL, NSBitmapImageFileType, NSDictionary *)>(method_getImplementation(pngMethod));
        QVERIFY(installHook(pngMethod, ^NSData *(id receiver, NSBitmapImageFileType type, NSDictionary *properties) {
            if (type == NSBitmapImageFileTypePNG) {
                if (_missingPng) {
                    return nil;
                }
                if (_invalidPng) {
                    return [@"Invalid PNG" dataUsingEncoding:NSUTF8StringEncoding];
                }
            }
            return pngOriginal(receiver, @selector(representationUsingType:properties:), type, properties);
        }));
    }

    void cleanup()
    {
        for (auto [method, original, replacement] : std::views::reverse(_hooks)) {
            method_setImplementation(method, original);
            imp_removeBlock(replacement);
        }
        _hooks.clear();
        _sourceImage = nil;
        _resizedImages = nil;
    }

    void missingImageReturnsEmptyIcon()
    {
        _sourceImage = nil;

        QVERIFY(OCC::loadAppIconFromBundle().isNull());
        QCOMPARE(_conversionCount, 0);
        QCOMPARE(static_cast<int>(_resizedImages.allObjects.count), 0);
    }

    void convertsImageAtEverySizeAndReleasesTemporaryImages()
    {
        const auto icon = OCC::loadAppIconFromBundle();

        QVERIFY(!icon.isNull());
        auto expectedSizes = QList<QSize>();
        for (const auto side : iconSides) {
            const auto size = QSize(side, side);
            expectedSizes.append(size * _imageScale);
            const auto image = icon.pixmap(size, 1.0).toImage();
            QCOMPARE(image.size(), size);
            QCOMPARE(image.pixelColor(side / 2, side / 2), QColor(Qt::white));
        }
        QCOMPARE(icon.availableSizes(), expectedSizes);
        verifyReleasedImages();
    }

    void missingTiffReleasesTemporaryImages()
    {
        _missingTiff = true;

        QVERIFY(OCC::loadAppIconFromBundle().isNull());
        verifyReleasedImages();
    }

    void missingPngReleasesTemporaryImages()
    {
        _missingPng = true;

        QVERIFY(OCC::loadAppIconFromBundle().isNull());
        verifyReleasedImages();
    }

    void invalidPngReleasesTemporaryImages()
    {
        _invalidPng = true;

        QVERIFY(OCC::loadAppIconFromBundle().isNull());
        verifyReleasedImages();
    }
};

QTEST_MAIN(TestMacAppIcon)
#include "testmacappicon.moc"

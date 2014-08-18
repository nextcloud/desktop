//
//  Finder.h
//
//  Created by snow on 9/30/10.
//  Copyright 2010 Canvastudio Les Nie. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>

struct _NSPoint {
    float x;
    float y;
};

struct _NSSize {
    float width;
    float height;
};

struct _NSRect {
    struct _NSPoint origin;
    struct _NSSize size;
};

typedef struct {
    struct TFENode *_M_start;
    struct TFENode *_M_finish;
    struct TFENode *_M_end_of_storage;
} _Vector_impl_6bc0f568;

struct TFENodeVector {
    _Vector_impl_6bc0f568 _M_impl;
};

@class TListViewController, TTableViewShrinkToFitController;

@interface TListView : NSOutlineView
{
    TListViewController *_controller;
    BOOL _itemHitOnMouseDown;
    TTableViewShrinkToFitController *_stfController;
}

- (void)dealloc;
- (void)setDelegate:(id)arg1;
- (BOOL)shouldDelayWindowOrderingForEvent:(id)arg1;
- (BOOL)acceptsFirstResponder;
- (id)columnWithStringIdentifier:(id)arg1;
- (struct CGRect)_dropHighlightBackgroundRectForRow:(long long)arg1;
- (void)drawRow:(long long)arg1 clipRect:(struct CGRect)arg2;
- (BOOL)clickedOnMoreButton:(id)arg1;
- (BOOL)handleUnicodeTextInput:(id)arg1;
- (BOOL)acceptsFirstMouse:(id)arg1;
- (unsigned long long)hitTestForEvent:(id)arg1 row:(long long)arg2;
- (id)menuForEvent:(id)arg1;
- (BOOL)_onlyDragOnContent;
- (BOOL)commonMouseDownAndEarlyReturn:(id)arg1 controller:(id)arg2;
- (void)commonPostMouseDown:(id)arg1 controller:(id)arg2;
- (BOOL)_typeSelectInterpretKeyEvent:(id)arg1;
- (void)mouseDown:(id)arg1;
- (void)drawRect:(struct CGRect)arg1;
- (BOOL)_wantsLiveResizeToUseCachedImage;
- (id)inputContext;
- (void)keyDown:(id)arg1;
- (void)expandItem:(id)arg1 expandChildren:(BOOL)arg2;
- (void)collapseItem:(id)arg1 collapseChildren:(BOOL)arg2;
- (void)selectRowIndexes:(id)arg1 byExtendingSelection:(BOOL)arg2;
- (void)editColumn:(long long)arg1 row:(long long)arg2 withEvent:(id)arg3 select:(BOOL)arg4;
- (id)preparedCellAtColumn:(long long)arg1 row:(long long)arg2;
- (void)startEditingWithNode:(const struct TFENode *)arg1;
- (void)stopEditing:(BOOL)arg1;
- (struct CGRect)maxSTFEditorFrameFromTitleFrame:(struct CGRect)arg1;
- (void)updateSTFEditorLocation;
- (BOOL)shrinkToFitTextViewAboutToOpen;
- (void)shrinkToFitTextViewEditingComplete:(id)arg1;
- (void)shrinkToFitTextViewAboutToClose;
- (id)stfEditorController;
@property(readonly, nonatomic) TListViewController *controller; // @synthesize controller=_controller;

@end

typedef struct {
    unsigned int selected:1;
    unsigned int focus:1;
    unsigned int twoLines:1;
    unsigned int label:3;
} CDStruct_b8373011;
typedef struct {
    struct CGRect _field1;
    unsigned long long _field2;
    struct CGRect _field3[2];
    unsigned long long _field4;
} CDStruct_51b97681;

@protocol IKImageProxy <NSObject>
- (void)bind;
- (void)unbind;
- (BOOL)isBinded;
- (int)proxyDataFormat;
- (id)proxyData;
- (void)disconnect;
- (void)connect:(id)fp8;
- (id)image;
- (id)thumbnailWithSize:(struct _NSSize)fp8 antialiased:(BOOL)fp16 qualityRequested:(int)fp20 qualityProduced:(int *)fp24;
- (BOOL)isVectorial;
- (struct _NSSize)proxySize;
- (void)lockForThreadedOperation;
- (void)unlockForThreadedOperation;
- (BOOL)isLockedForThreadedOperation;
@end


@interface IKImageWrapper : NSObject
{
    NSString *_path;
    NSData *_dataRepresentation;
    NSBitmapImageRep *_bitmapRepresentation;
    id <IKImageProxy> _imageProxy;
    union {
        struct CGImage *_cgImage;
        CIImage *_ciImage;
        struct CGImageSource *_cgImageSource;
        NSImage *_nsImage;
    } _volatileRep;
    unsigned short _volatileRepresentation;
    unsigned int _exifOrientation:3;
    unsigned int _generatedWithIconServices:1;
    unsigned int _underlyingDataAreVolatile:1;
    unsigned int _isReference:1;
    struct _NSSize _cachedSize;
    NSDictionary *_info;
}

+ (id)imageWithPath:(id)fp8;
+ (id)imageWithNSImage:(id)fp8;
+ (id)imageWithCGImage:(struct CGImage *)fp8;
+ (id)imageWithCGImageSource:(struct CGImageSource *)fp8;
+ (id)imageWithData:(id)fp8;
+ (id)imageWithPasteboard:(id)fp8;
+ (id)imageWithNSBitmapImageRep:(id)fp8;
+ (id)emptyImage;
+ (id)imageWithSize:(struct _NSSize)fp8;
+ (id)imageWithImageProxy:(id)fp8;
+ (id)imageWithObject:(id)fp8;
- (void)dealloc;
- (void)finalize;
- (id)initWithPath:(id)fp8;
- (id)initWithCGImage:(struct CGImage *)fp8;
- (id)initWithCGImageSource:(struct CGImageSource *)fp8;
- (id)initWithNSImage:(id)fp8;
- (id)initEmptyImage;
- (id)initWithPasteboard:(id)fp8;
- (id)initWithSize:(struct _NSSize)fp8;
- (id)initWithData:(id)fp8;
- (id)initWithNSBitmapImageRep:(id)fp8;
- (id)initWithImageProxy:(id)fp8;
- (id)initWithOpenGLID:(unsigned int)fp8 size:(struct _NSSize)fp12 offset:(struct _NSPoint)fp20 premultiplied:(BOOL)fp28 deleteWhenDone:(BOOL)fp32;
- (int)volatileRepresentation;
- (void)setVolatileRepresentation:(int)fp8;
- (void)releaseVolatileImageRep;
- (unsigned short)flags;
- (void)setFlags:(unsigned short)fp8;
- (BOOL)wasGeneratedWithIconServices;
- (void)setWasGeneratedWithIconServices:(BOOL)fp8;
- (BOOL)underlyingDataAreVolatile;
- (void)setUnderlyingDataAreVolatile:(BOOL)fp8;
- (struct CGImage *)_cgImage;
- (id)_nsImage;
- (struct CGImage *)cgImage;
- (id)nsImage:(BOOL)fp8;
- (id)nsImage;
- (struct CGImageSource *)cgImageSourceRef:(BOOL)fp8;
- (void)setCGImageSource:(struct CGImageSource *)fp8;
- (void)setCGImage:(struct CGImage *)fp8;
- (void)setNSImage:(id)fp8;
- (id)copy;
- (void)setIsReference:(BOOL)fp8;
- (void)integrateReferenceInstance:(id)fp8;
- (void)referenceWillDie;
- (id)referenceInstance;
- (id)_thumbnailWithSize:(struct _NSSize)fp8 antialiased:(BOOL)fp16 qualityRequested:(int)fp20 qualityProduced:(int *)fp24;
- (id)thumbnailWithSize:(struct _NSSize)fp8 antialiased:(BOOL)fp16 qualityRequested:(int)fp20 qualityProduced:(int *)fp24;
- (struct _NSSize)cachedSize;
- (id)_sizeOfNSImage:(id)fp8;
- (struct _NSSize)_size;
- (struct _NSSize)size;
- (void)setSize:(struct _NSSize)fp8;
- (void)setSizeWithoutSavingContent:(struct _NSSize)fp8;
- (BOOL)isVectorial;
- (BOOL)isValid;
- (BOOL)isEmpty;
- (BOOL)hasAlpha;
- (id)animatedGifsCache;
- (BOOL)isAnimatedGifs;
- (int)imageFrameCount;
- (int)loopCount;
- (float)nextFrameDelayAtIndex:(int)fp8;
- (struct CGImage *)imageAtFrameIndex:(int)fp8;
- (id)GIFRepresentation;
- (id)TIFFRepresentation;
- (id)TIFFRepresentationUsingCompression:(unsigned int)fp8 factor:(float)fp12;
- (id)IK_JPEGRepresentationWithCompressionFactor:(float)fp8;
- (id)imagePath;
- (id)_dataRepresentationFromBitmapRepresentation:(id)fp8;
- (id)_createBitmapImageRepFromCGRepresentation;
- (id)dataRepresentationFromCGRepresentationWithCompressionFactor:(float)fp8;
- (id)dataRepresentation;
- (id)imageProxy;
- (void)setImageProxy:(id)fp8;
- (void)setPath:(id)fp8;
- (void)setDataRepresentation:(id)fp8;
- (void)drawInRect:(NSRect)fp8 fromRect:(NSRect)fp24 alpha:(float)fp40;
- (void)lockFocus;
- (void)unlockFocus;
- (void)saveAsTIFFAtPath:(id)fp8;
- (void)saveAsJPGAtPath:(id)fp8;
- (id)writeToFileWithAutomaticFormat:(id)fp8;
- (BOOL)hasDataRepresentation;
- (BOOL)hasBitmapRepresentation;
- (id)bitmapRepresentation;
- (void)setBitmapRepresentation:(id)fp8;
- (struct CGContext *)cgContext;
- (void)mapIntoVRAM;
- (void)mapRepresentationIntoRAM:(int)fp8;
- (BOOL)mappedIntoRAM;
- (BOOL)mappedAndDecompressedIntoRAM;
- (BOOL)mappedIntoVRAM;
- (void)freeImageCache;
- (void)bindCGCache;
- (BOOL)hasCGCache;
- (BOOL)hasVolatileCache;
- (BOOL)hasRAMCache;
- (void)freeRAMCache;
- (void)freeVRAMCache;
- (void)freeCache;
- (BOOL)textureIsPacked;
- (unsigned int)openGLTextureID;
- (void)deleteTextureInCurrentContext;
- (void)setOpenGLTextureID:(unsigned int)fp8 withGLContext:(id)fp12;
- (unsigned int)generateNewGLTextureID;
- (struct _NSPoint)openGLTextureOffset;
- (void)setOpenGLTextureOffset:(struct _NSPoint)fp8;
- (BOOL)openGLTextureIsPremultiplied;
- (void)setOpenGLTextureIsPremultiplied:(BOOL)fp8;
- (void)setValue:(id)fp8 forKey:(id)fp12;
- (id)valueForKey:(id)fp8;
- (id)_tryToCreateCGImageRepFromNonCGFile:(id)fp8;
- (id)description;

@end


@class IKMipmapItem;

@interface IKMipmapImage : NSObject
{
    IKMipmapItem *_mipmaps[4];
    IKMipmapItem *_originalMipmap;
    IKMipmapItem *_customMipmap;
    unsigned long _version;
    struct _NSSize _originalImageSizeCache;
    unsigned int _originalImageIsInvalid:1;
    unsigned int _dirty:1;
    unsigned int _mark:1;
    unsigned int _isReference:1;
}

- (void)_mipmapCommonInit;
- (id)init;
- (id)initWithMipmapSizes:(id)fp8 VMUsagePolicy:(id)fp12;
- (void)_cleanUp;
- (void)dealloc;
- (void)finalize;
- (BOOL)isDirty;
- (void)setDirty:(BOOL)fp8;
- (void)setIsReference:(BOOL)fp8;
- (void)setOriginalImageIsInvalid:(BOOL)fp8;
- (unsigned long)version;
- (void)setVersion:(unsigned long)fp8;
- (BOOL)marked;
- (void)mark;
- (void)clearMark;
- (id)temporaryItem;
- (id)originalItem;
- (id)mipmapItemAtIndex:(int)fp8;
- (int)indexOfMipmapItem:(id)fp8;
- (int)highestMipmapItemIndex;
- (id)highestMipmapItem;
- (float)originalAspectRatio;
- (BOOL)originalImageIsInvalid;
- (void)checkAndMarkMipmapAsInvalid;
- (void)invalidateOriginalImageSizeCache;
- (struct _NSSize)originalImageSize;
- (void)setOriginalImageSizeCache:(struct _NSSize)fp8;
- (struct _NSSize)originalImageSizeCache;
- (id)image;
- (void)setImage:(id)fp8;
- (void)setImageWithoutInvalidate:(id)fp8;
- (id *)mipmapItems;
- (id)validMipmapItems;
- (BOOL)atLeastOneMipmapItemIsValid;
- (BOOL)allMipmapItemsAreValid;
- (BOOL)customMipmapIsValidAndMatchSize:(struct _NSSize)fp8 andQuality:(int)fp16;
- (BOOL)validateMipmap:(id)fp8 withModel:(id)fp12 withQuality:(int)fp16;
- (BOOL)validateMipmap:(id)fp8 withQuality:(int)fp12;
- (BOOL)validateMipmapAtIndex:(int)fp8 withQuality:(int)fp12;
- (int)bestMipmapIndexToValidateForSize:(int)fp8;
- (id)mipmapWithSize:(int)fp8;
- (void)setImage:(id)fp8 forMipmapSize:(int)fp12;
- (id)fastMipmapItemForSize:(int)fp8 forOpenGL:(BOOL)fp12 useMinimumQualityThreshold:(BOOL)fp16;
- (id)_fastMipmapItemForSize:(struct _NSSize)fp8 forOpenGL:(BOOL)fp16 useMinimumQualityThreshold:(BOOL)fp20;
- (id)fastMipmapItemForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16;
- (id)fastestMipmapItemForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16;
- (void)_cacheMipmapSize:(struct _NSSize)fp8 fromModel:(id)fp16;
- (id)nicestMipmapItemForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16 cacheIt:(BOOL)fp20;
- (BOOL)shouldUseOriginalImageToCacheNiceImageWithSize:(struct _NSSize)fp8;
- (id)niceMipmapItemForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16 cacheIt:(BOOL)fp20;
- (id)fastMipmapItemWithExactSize:(struct _NSSize)fp8;
- (id)nicestImageForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16 cacheIt:(BOOL)fp20;
- (id)niceImageForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16 cacheIt:(BOOL)fp20;
- (id)fastImageForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16;
- (id)fastestImageForSize:(struct _NSSize)fp8 forGLRendering:(BOOL)fp16;
- (id)lockMipmapAtIndex:(int)fp8;
- (void)unlockMipmapItem:(id)fp8;
- (BOOL)preloadMipmapsWithQuality:(int)fp8;
- (BOOL)containsMipmapItem:(id)fp8;
- (void)freeAllCaches;
- (void)freeTemporaryCache;
- (void)freeExpendedRepresentationCaches;
- (void)freeOriginalImageCache;
- (void)invalidateMipMaps;
- (void)setMipmapSizes:(id)fp8;
- (void)setMipmapVMUsagePolicy:(id)fp8;
- (id)referenceInstance;
- (void)integrateReferenceInstance:(id)fp8;
- (void)referenceWillDie;

@end


@interface IKMipmapItem : NSObject
{
    IKMipmapImage *_parent;
    IKImageWrapper *_image;
    int _mipmapSize;
    unsigned int _vmUsagePolicy:8;
    unsigned int _thumbnailQuality:8;
    unsigned int _isReference:1;
}

- (id)init;
- (void)dealloc;
- (id)description;
- (id)parent;
- (void)setParent:(id)fp8;
- (BOOL)loaded;
- (void)mapIntoVRAM;
- (BOOL)unload;
- (BOOL)isValid;
- (int)thumbnailQuality;
- (void)setThumbnailQuality:(int)fp8;
- (id)__image;
- (id)image;
- (void)setImage:(id)fp8;
- (void)setMipmapSize:(int)fp8;
- (void)invalidate;
- (int)mipmapSize;
- (void)__setDictionaryRepresentation:(id)fp8;
- (BOOL)setAsMipmapOfImage:(id)fp8 withSize:(struct _NSSize)fp12 antialiased:(BOOL)fp20 quality:(int)fp24;
- (BOOL)setAsMipmapOfImage:(id)fp8 aspectRatio:(float)fp12 antialiased:(BOOL)fp16 quality:(int)fp20;
- (int)vmUsagePolicy;
- (void)setVmUsagePolicy:(int)fp8;
- (void)setIsReference:(BOOL)fp8;
- (void)setAsReferenceOf:(id)fp8;
- (void)integrateReferenceInstance:(id)fp8 replaceImage:(BOOL)fp12;
- (void)referenceWillDie;

@end


@interface IKImageCell : NSObject
{
    id _parent;
    id _cellSource;
    id _proxy;
    unsigned int _dataSourceIndex;
    unsigned int _mipmapDBIndex;
    IKMipmapImage *_mipmapImage;
    unsigned int _datasourceIsVectorial:1;
    float _alpha;
    NSMutableDictionary *_properties;
}

+ (id)_IKBuildImageWrapperForType:(id)fp8 withObject:(id)fp12 withOwner:(id)fp16;
- (id)init;
- (struct _NSRect)imageFrame;
- (void)invalidate;
- (void)validate;
- (void)mipmapImageChanged;
- (void)validateMipmapDBIndex;
- (id)mipmapDB;
- (unsigned int)mipmapDBIndex;
- (void)setCacheDBIndex:(unsigned int)fp8;
- (void)parentWillDie:(id)fp8;
- (void)dealloc;
- (void)finalize;
- (void)setParent:(id)fp8;
- (id)parent;
- (void)setDataSource:(id)fp8;
- (id)dataSource;
- (unsigned int)dataSourceIndex;
- (void)setDataSourceIndex:(unsigned int)fp8;
- (id)mipmapImage;
- (void)setMipmapImage:(id)fp8;
- (float)alpha;
- (void)setAlpha:(float)fp8;
- (BOOL)isAnIcon;
- (BOOL)_representationTypeCanBePlayed:(id)fp8;
- (void)removeObjectForKey:(id)fp8;
- (void)setObject:(id)fp8 forKey:(id)fp12;
- (id)objectForKey:(id)fp8;
- (void)checkMipmapVersion;
- (id)dataSourcePath;

@end

//@class IKImageWrapper;

@interface TIconViewCell : IKImageBrowserCell
{
    IKImageWrapper *_titleImage;
    BOOL _twoLine;
    CDStruct_b8373011 _titleSettings;
}

+ (struct CGSize)cellSizeForIconSize:(double)arg1 labelOnBottom:(BOOL)arg2 gridSpacing:(double)arg3 titleAttrs:(id)arg4 subTitleAttrs:(id)arg5;
- (id)init;
- (void)dealloc;
- (double)iconSize;
- (BOOL)labelOnBottom;
- (BOOL)showPreview;
- (BOOL)showItemInfo;
- (double)endCapWidth;
- (struct CGRect)frame;
- (struct CGRect)imageFrame;
- (double)titleOffset;
- (double)maxTitleWidth;
- (struct CGRect)titleFrame;
- (struct CGRect)subtitleFrame;
- (int)heightOfInfoSpace;
- (id)subString:(id)arg1 atIndex:(unsigned long long)arg2 attributes:(id)arg3 lineBreakMode:(unsigned long long)arg4;
- (struct CGRect)placeSubString:(id)arg1 atIndex:(unsigned long long)arg2 fromFrame:(struct CGRect)arg3 bounds:(struct CGRect)arg4 attributes:(id)arg5 lineBreakMode:(unsigned long long)arg6 position:(BOOL)arg7;
- (CDStruct_51b97681)calculateTextMetrics:(id)arg1 attributes:(id)arg2;
- (void)drawLabel:(struct CGContext *)arg1 fillRect:(struct CGRect)arg2 bounds:(struct CGRect)arg3 firstLine:(struct CGRect)arg4 secondLine:(struct CGRect)arg5 actualLineCount:(unsigned long long)arg6 selected:(BOOL)arg7 labelValue:(short)arg8 justification:(short)arg9 inset:(double)arg10 radius:(double)arg11;
- (id)constructTitleImage;
- (CDStruct_b8373011)currentTitleImageSettings;
- (BOOL)titleImageStillValid:(CDStruct_b8373011)arg1;
- (id)titleImage;
- (void)invalidate;
- (void)drawTitle;

@end

@protocol TShrinkToFitDelegateProtocol
- (BOOL)shrinkToFitTextViewAboutToOpen;
- (void)shrinkToFitTextViewEditingComplete:(id)arg1;
- (void)shrinkToFitTextViewAboutToClose;
@end

@class TCocoaShrinkToFitController, TIconViewController;

@interface TIconView : IKImageBrowserView <TShrinkToFitDelegateProtocol>
{
    TIconViewController *_controller;
    TCocoaShrinkToFitController *_stfController;
    BOOL _startEditingOnMouseUp;
    BOOL _viewIsReloadingData;
    BOOL _isDrawingInDragImage;
    unsigned long long _editedCellIndex;
    unsigned long long _selectionCountBeforeReloadingData;
}

- (id)initWithFrame:(struct CGRect)arg1 controller:(id)arg2;
- (void)setDelegate:(id)arg1;
- (id)_viewIdentifier;
- (void)viewDidMoveToWindow;
- (BOOL)shouldPreserveVisibleRangeWhileZooming;
- (void)reloadData;
- (BOOL)isReloadingData;
- (unsigned long long)selectionCountBeforeReloadingData;
- (BOOL)respondsToSelector:(SEL)arg1;
- (void)draggingExited:(id)arg1;
- (void)dragImage:(id)arg1 at:(struct CGPoint)arg2 offset:(struct CGSize)arg3 event:(id)arg4 pasteboard:(id)arg5 source:(id)arg6 slideBack:(BOOL)arg7;
- (void)scrollSTFEditorIntoView;
- (void)updateSTFEditorLocation;
- (void)scrollWheel:(id)arg1;
- (void)browserDidScroll;
- (void)mouseDown:(id)arg1;
- (void)drawRect:(struct CGRect)arg1;
- (void)rightMouseDown:(id)arg1;
- (void)mouseDragged:(id)arg1;
- (void)mouseUp:(id)arg1;
- (BOOL)_typeSelectInterpretKeyEvent:(id)arg1;
- (id)inputContext;
- (void)keyDown:(id)arg1;
- (int)defaultHeightOfInfoSpaceWithCurrentViewOptions;
- (BOOL)isDragImageOpaque;
- (int)nextIndexInGridLayoutWithDirectionKey:(unsigned short)arg1 currentIndex:(long long)arg2;
- (void)startEditingWithNode:(const struct TFENode *)arg1 afterDelay:(BOOL)arg2;
- (void)stopEditing:(BOOL)arg1;
- (unsigned long long)editingIndex;
- (struct CGRect)maxSTFEditorFrameForCellAtIndex:(unsigned long long)arg1;
- (BOOL)editCellTitleAtIndex:(unsigned long long)arg1 withEvent:(id)arg2 select:(BOOL)arg3;
- (BOOL)shrinkToFitTextViewAboutToOpen;
- (void)shrinkToFitTextViewEditingComplete:(id)arg1;
- (void)shrinkToFitTextViewAboutToClose;
- (BOOL)hasFocus;
- (id)draggedImageWithEvent:(id)arg1 countBadge:(int)arg2 hotPoint:(struct CGPoint *)arg3;
@property(readonly, retain, nonatomic) TIconViewController *controller; // @synthesize controller=_controller;

@end

@class TPropertyIconController;

@interface TIconImageView : NSImageView
{
    TPropertyIconController *_controller;
}

- (id)initWithFrame:(struct CGRect)arg1;
- (id)initWithCoder:(id)arg1;
- (void)initCommon;
- (void)drawRect:(struct CGRect)arg1;
- (void)setImage:(id)arg1;
- (BOOL)canChangeIcon;
- (BOOL)validateCopy:(id)arg1;
- (void)copy:(id)arg1;
- (BOOL)validateCut:(id)arg1;
- (void)cut:(id)arg1;
- (BOOL)validateDelete:(id)arg1;
- (void)delete:(id)arg1;
- (BOOL)validatePaste:(id)arg1;
- (void)paste:(id)arg1;
- (BOOL)validateUndo:(id)arg1;
- (void)undo:(id)arg1;
- (BOOL)validateRedo:(id)arg1;
- (void)redo:(id)arg1;
- (BOOL)validateMenuItem:(id)arg1;
- (unsigned long long)draggingEntered:(id)arg1;
- (void)concludeDragOperation:(id)arg1;
- (void)draggingEnded:(id)arg1;
@property TPropertyIconController *controller;  //@synthesize controller=_controller;

@end

@interface FINode : NSObject
{
    
}

+ (id)nodeWithFENode:(const struct TFENode *)arg1;
+ (struct TFENode)asFENode:(id)arg1;
- (struct TFENode)feNode;
- (struct TFENode)feNodeFollowingAliasChainSynchronously;
- (struct TFENode)feNodeFollowingAliasChainAsyncWithTarget:(id)arg1 okToLogin:(BOOL)arg2 tryToFixIfBroken:(BOOL)arg3;
- (BOOL)nodeIs:(unsigned long long)arg1;
- (id)name;
- (id)fullPath;
- (id)kind;
- (id)kindWithoutPlatform;
- (id)copyMDAttribute:(struct __CFString *)arg1;
- (id)typeIdentifier;
- (short)labelValue;
- (id)icon;
- (BOOL)isDimmed;
- (id)modificationDate;
- (id)creationDate;
- (id)lastOpenedDate;
- (long long)fileSize;
- (id)size:(BOOL)arg1;
- (id)label;
- (id)version;
- (id)comments;
- (id)authorName;
- (id)serverUserName;
- (BOOL)supportsScreenSharing;
- (BOOL)supportsFileSharing;
- (int)serverConnectionState;
- (BOOL)isSharedServer;
- (BOOL)isODSNode;
- (BOOL)isMountedSharePoint;
- (BOOL)isIDiskNode;
- (BOOL)isVolume;
- (BOOL)volumeIsEjectableOrUnmountable;
- (void)connectToSharedServerAs;
- (void)askToUseODS;
- (void)disconnectShare;
- (void)launchScreenSharingApp;
- (id)url;
- (long long)fileSizeSync;
- (BOOL)isExtensionHidden;
- (BOOL)containsLocalizations;
- (BOOL)containsPlugins;
- (BOOL)isAlias;
- (BOOL)isMDQueryHit;
- (BOOL)isResolved;
- (BOOL)isApplication;
- (BOOL)isContainer;
- (BOOL)isPackage;
- (BOOL)isVirtual;
- (BOOL)isQueryHit;
- (unsigned long long)nodeIs64:(unsigned long long)arg1;

@end


@interface TViewController : NSViewController
{
}

- (id)initWithCoder:(id)arg1;
- (id)initWithNibName:(id)arg1 bundle:(id)arg2;
- (void)initCommon;
- (void)loadView;

@end

@class IPropertyValueExtractor, NSObject, TLayoutBinder;

@interface IPropertyValueController : TViewController
{
    NSObject *_value;
    TLayoutBinder *_layoutBinder;
    double _viewHeight;
    IPropertyValueExtractor *_valueExtractor;
    BOOL _shouldBeVisible;
    BOOL _shouldBeEnabled;
}

+ (id)propertyValueController;
+ (id)propertyValueControllerWithValueExtractor:(id)arg1;
- (id)initWithValueExtractor:(id)arg1;
- (void)initCommon;
- (void)dealloc;
- (id)defaultValue;
- (void)setView:(id)arg1;
@property(retain) IPropertyValueExtractor *valueExtractor; // @synthesize valueExtractor=_valueExtractor;
- (void)updateWithNodes:(const struct TFENodeVector *)arg1;
@property BOOL shouldBeVisible; // @synthesize shouldBeVisible=_shouldBeVisible;
- (id)extractValueFromNodes:(const struct TFENodeVector *)arg1;
- (BOOL)needsUpdateForProperty:(unsigned int)arg1;
- (BOOL)isApplicableToNodes:(const struct TFENodeVector *)arg1;
- (void)flush;
- (BOOL)canModifyNodes:(const struct TFENodeVector *)arg1;
- (BOOL)adjustSize:(BOOL)arg1;
- (void)handleNodesGoingAway:(const struct TFENodeVector *)arg1;
- (void)handleNodeMDAttributesChanged:(const struct TFENode *)arg1 attributes:(id)arg2 isDisplayAttributes:(BOOL)arg3;
@property BOOL shouldBeEnabled; // @synthesize shouldBeEnabled=_shouldBeEnabled;
@property(readonly, retain) TLayoutBinder *layoutBinder; // @synthesize layoutBinder=_layoutBinder;
@property(retain) NSObject *value; // @synthesize value=_value;

@end


@interface TPropertyImageViewController : IPropertyValueController
{
}

@end



@interface TPropertyIconController : TPropertyImageViewController
{
    struct TFENodeVector _nodes;
    BOOL _nodesHaveSameIcon;
    BOOL _nodesHaveCustomIcon;
    BOOL _nodesCanChangeIcon;
}

- (void)initCommon;
- (void)updateWithNodes:(const struct TFENodeVector *)arg1;
- (BOOL)canModifyNodes:(const struct TFENodeVector *)arg1;
- (BOOL)validateCopy:(id)arg1;
- (void)copy:(id)arg1;
- (BOOL)validateCut:(id)arg1;
- (void)cut:(id)arg1;
- (BOOL)validateDelete:(id)arg1;
- (void)delete:(id)arg1;
- (BOOL)validatePaste:(id)arg1;
- (void)paste:(id)arg1;
- (void)concludeDragOperation:(id)arg1;

@end

struct TFENode {
    struct OpaqueNodeRef *fNodeRef;
};


@class TViewOptionsWindowController;

@interface TFileBasedImageView : NSImageView
{
    TViewOptionsWindowController *_controller;
    struct TFENode _imageNode;
}

@property struct TFENode *imageNode; // @dynamic imageNode;
- (void)mouseDown:(id)arg1;
- (BOOL)performDragOperation:(id)arg1;

@end

@interface TTextCell : NSTextFieldCell
{
    double _leftMargin;
    double _rightMargin;
    BOOL _drawGrayTextWhenDisabled;
}

- (id)init;
- (id)initTextCell:(id)arg1;
- (id)initWithCoder:(id)arg1;
- (void)initializeTextCell;
- (struct CGSize)cellSizeForBounds:(struct CGRect)arg1;
- (struct CGRect)titleRectForBounds:(struct CGRect)arg1;
- (void)drawInteriorWithFrame:(struct CGRect)arg1 inView:(id)arg2;
- (void)drawWithExpansionFrame:(struct CGRect)arg1 inView:(id)arg2;
- (unsigned long long)hitTestForEvent:(id)arg1 inRect:(struct CGRect)arg2 ofView:(id)arg3;
@property BOOL drawGrayTextWhenDisabled; // @synthesize drawGrayTextWhenDisabled=_drawGrayTextWhenDisabled;
@property double rightMargin; // @synthesize rightMargin=_rightMargin;
@property double leftMargin; // @synthesize leftMargin=_leftMargin;

@end

struct TIconRef {
    //struct TRef fIconRef;
};

@interface TIconAndTextCell : TTextCell
{
    struct TIconRef _icon;
    struct CGSize _iconSize;
    double _iconToTextSpacing;
    BOOL _showIcon;
}

- (void)initializeTextCell;
- (id)copyWithZone:(struct _NSZone *)arg1;
- (void)setIcon:(const struct TIconRef *)arg1;
@property(readonly) struct TIconRef *icon;
- (struct CGRect)titleRectForBounds:(struct CGRect)arg1;
- (struct CGRect)imageRectForBounds:(struct CGRect)arg1;
- (struct CGSize)cellSizeForBounds:(struct CGRect)arg1;
- (void)drawIconWithFrame:(struct CGRect)arg1;
- (void)drawInteriorWithFrame:(struct CGRect)arg1 inView:(id)arg2;
- (unsigned long long)hitTestForEvent:(id)arg1 inRect:(struct CGRect)arg2 ofView:(id)arg3;
@property BOOL showIcon; // @synthesize showIcon=_showIcon;
@property double iconToTextSpacing; // @synthesize iconToTextSpacing=_iconToTextSpacing;
@property struct CGSize iconSize; // @synthesize iconSize=_iconSize;

@end

@interface TNodeIconAndNameCell : TIconAndTextCell
{
    struct TFENode _node;
}

- (id)copyWithZone:(struct _NSZone *)arg1;
- (const struct TFENode *)node;
- (void)setNode:(const struct TFENode *)arg1;
- (id)accessibilityAttributeNames;
- (id)accessibilityAttributeValue:(id)arg1;
- (BOOL)accessibilityIsAttributeSettable:(id)arg1;

@end

@class NSImage, NSView;

@interface TListViewIconAndTextCell : TNodeIconAndNameCell
{
    NSImage *_thumbnail;
    NSView *_view;
}

- (void)initializeTextCell;
- (void)dealloc;
- (id)copyWithZone:(struct _NSZone *)arg1;
- (void)drawIconWithFrame:(struct CGRect)arg1;
- (id)controller;
- (id)accessibilityActionNames;
- (id)accessibilityActionDescription:(id)arg1;
- (void)accessibilityPerformAction:(id)arg1;
@property NSView *view; // @synthesize view=_view;
@property(retain) NSImage *thumbnail; // @synthesize thumbnail=_thumbnail;

@end

@interface IKImageFlowView : NSOpenGLView
{
    id _dataSource;
    id _dragDestinationDelegate;
    id _delegate;
    void *_reserved;
}

+ (id)pixelFormat;
+ (BOOL)flowViewIsSupportedByCurrentHardware;
+ (void)initialize;
+ (void)setImportAnimationStyle:(unsigned int)fp8;
- (void)_setDefaultTextAttributes;
- (void)_ikCommonInit;
- (id)initWithFrame:(struct _NSRect)fp8;
- (void)dealloc;
- (void)finalize;
- (void)setValue:(id)fp8 forUndefinedKey:(id)fp12;
- (id)valueForUndefinedKey:(id)fp8;
- (id)allocateNewCell;
- (void)dataSourceDidChange;
- (void)_reloadCellDataAtIndex:(int)fp8;
- (void)reloadCellDataAtIndex:(int)fp8;
- (void)reloadAllCellsData;
- (void)reloadData;
- (id)loadCellAtIndex:(int)fp8;
- (void)didStabilize;
- (BOOL)isAnimating;
- (void)setAnimationsMask:(unsigned int)fp8;
- (unsigned int)animationsMask;
- (void)_cellFinishedImportAnimation:(id)fp8;
- (BOOL)itemAtIndexIsLoaded:(unsigned int)fp8;
- (void)keyWindowChanged:(id)fp8;
- (void)setSelectedIndex:(unsigned int)fp8;
- (BOOL)hitTestWithImage:(id)fp8 x:(float)fp12 y:(float)fp16;
- (unsigned int)cellIndexAtLocation:(struct _NSPoint)fp8;
- (void)_adjustScroller;
- (void)resetCursorRects;
- (void)frameDidChange:(id)fp8;
- (void)invalidateLayout;
- (float)offset;
- (int)cellIndexAtPosition:(float)fp8;
- (int)heightOfInfoSpace;
- (int)countOfVisibleCellsOnEachSide;
- (struct _NSRange)rangeOfVisibleIndexes;
- (struct _NSRange)rangeOfVisibleIndexesAtSelection;
- (id)visibleCellIndexesAtSelection;
- (id)visibleCellIndexes;
- (void)flipCellsWithOldSelectedIndex:(unsigned int)fp8 newSelectedIndex:(unsigned int)fp12;
- (void)flowLayout:(struct _NSRange)fp8;
- (void)zoomOnSelectedLayerLayout:(struct _NSRange)fp8;
- (void)updateLayoutInRange:(struct _NSRange)fp8;
- (void)updateLayout;
- (struct _NSRect)titleFrame;
- (struct _NSRect)subtitleFrame;
- (struct _NSRect)splitterFrame;
- (double)_viewAspectRatio;
- (double)_zScreen;
- (struct _NSSize)imageRenderedSize;
- (struct _NSRect)selectedImageFrame;
- (double)_computeCameraDZ;
- (double)cameraDZ;
- (double)_computeCameraDY;
- (double)cameraDY;
- (float)convertPixelUnitTo3DUnit:(float)fp8;
- (double)alignOnPixelValue;
- (BOOL)updatesCGSurfaceOnDrawRect;
- (void)setUpdatesCGSurfaceOnDrawRect:(BOOL)fp8;
- (BOOL)showSplitter;
- (void)setShowSplitter:(BOOL)fp8;
- (id)delegate;
- (void)setDelegate:(id)fp8;
- (id)dataSource;
- (void)setDataSource:(id)fp8;
- (void)setZoomOnSelectedLayer:(BOOL)fp8;
- (BOOL)zoomOnSelectedLayer;
- (unsigned int)itemsCount;
- (id)cells;
- (unsigned int)selectedIndex;
- (unsigned int)focusedIndex;
- (id)backgroundColor;
- (void)_setBackgroundColorWithRed:(float)fp8 green:(float)fp12 blue:(float)fp16 alpha:(float)fp20;
- (BOOL)backgroundIsLight;
- (BOOL)backgroundIsBlack;
- (BOOL)_convertColor:(id)fp8 toRed:(float *)fp12 green:(float *)fp16 blue:(float *)fp20 alpha:(float *)fp24;
- (void)_getBackgroundRed:(float *)fp8 green:(float *)fp12 blue:(float *)fp16 alpha:(float *)fp20;
- (void)setBackgroundColor:(id)fp8;
- (id)cellBackgroundColor;
- (void)setCellBackgroundColor:(id)fp8;
- (id)cellBorderColor;
- (void)setCellBorderColor:(id)fp8;
- (float)imageAspectRatio;
- (void)setImageAspectRatio:(float)fp8;
- (float)scaleFactor;
- (id)cacheManager;
- (BOOL)cellsAlignOnBaseline;
- (void)setCellsAlignOnBaseline:(BOOL)fp8;
- (void)startInlinePreview;
- (void)stopInlinePreview;
- (void)inlinePreviewDidRenderImage:(void *)fp8;
- (id)thumbnailImageAtIndex:(int)fp8;
- (id)previewImageAtIndex:(int)fp8;
- (void)initRenderingContext;
- (void *)fogShader;
- (void)renewGState;
- (void)setHidden:(BOOL)fp8;
- (id)renderer;
- (void)_setAutoscalesBoundsToPixelUnits:(BOOL)fp8;
- (void)setCacheManager:(id)fp8;
- (id)imageFlowContext;
- (void)setImageFlowContext:(id)fp8;
- (void)__ikSetupGLContext:(id)fp8;
- (id)openGLContext;
- (void)setOpenGLContext:(id)fp8;
- (void)_cacheWasFlushed:(id)fp8;
- (float)fogAtLocation:(float)fp8;
- (struct _NSRect)clampedBounds;
- (struct _NSRect)clampedFrame;
- (void)drawVisibleCells:(struct _NSRect)fp8;
- (void)drawBackground;
- (void)drawTitle;
- (BOOL)installViewport;
- (void)setupGLState;
- (void)installPerspetiveViewportForPicking:(BOOL)fp8 location:(struct _NSPoint)fp12;
- (void)drawFocusRing;
- (BOOL)drawWithCurrentRendererInRect:(struct _NSRect)fp8;
- (void)__copyPixels:(void *)fp8 withSize:(struct _NSSize)fp12 toCurrentFocusedViewAtPoint:(struct _NSPoint)fp20;
- (void)__copyGLToCurrentFocusedView;
- (BOOL)_createPBuffer;
- (void)_deletePBUffer;
- (BOOL)_installPBuffer;
- (void)_copyPBufferToCGSurface;
- (void)drawRect:(struct _NSRect)fp8;

@end

@interface TFlowView : IKImageFlowView
{
}

- (id)_viewIdentifier;
- (BOOL)acceptsFirstMouse:(id)arg1;
- (BOOL)acceptsFirstResponder;
- (void)dragImage:(id)arg1 at:(struct CGPoint)arg2 offset:(struct CGSize)arg3 event:(id)arg4 pasteboard:(id)arg5 source:(id)arg6 slideBack:(BOOL)arg7;
- (BOOL)isDragImageOpaque;
- (void)mouseDown:(id)arg1;
- (void)reloadData;
- (void)resetCursorRects;
- (BOOL)shouldDelayWindowOrderingForEvent:(id)arg1;

@end

@interface TContextMenu : NSMenu
{
}

+ (id)contextMenuWithDelegate:(id)arg1;
+ (void)clearContextMenuState;
+ (BOOL)allowContextualMenuForEvent:(id)arg1;
+ (void)contextMenuClickedOnNodes:(const struct TFENodeVector *)arg1 event:(id)arg2 view:(id)arg3 windowController:(id)arg4;
+ (void)contextMenuClickedOnContainer:(const struct TFENode *)arg1 event:(id)arg2 view:(id)arg3 windowController:(id)arg4;
+ (void)populateActionMenu:(id)arg1 forWindowController:(id)arg2;
- (id)initWithTitle:(id)arg1;
- (id)initWithObject:(id)arg1 nodes:(const struct TFENodeVector *)arg2 event:(id)arg3;
- (id)initWithDelegate:(id)arg1;
- (void)dealloc;
- (void)configureWithNodes:(const struct TFENodeVector *)arg1 windowController:(id)arg2 container:(BOOL)arg3;
- (void)configureForSidebarWithNode:(const struct TFENode *)arg1 windowController:(id)arg2 constrained:(BOOL)arg3 data:(id)arg4;
- (void)configureForPathbarWithNode:(const struct TFENode *)arg1 windowController:(id)arg2;
- (void)menuDidCompleteInteraction:(id)arg1;

@end

@interface TContextMenu (Private)
+ (void)addViewSpecificStuffToMenu:(id)arg1 browserViewController:(id)arg2 context:(unsigned int)arg3;
+ (void)buildContextMenu:(id)arg1 forContext:(unsigned int)arg2 target:(id)arg3 maxItems:(unsigned long long)arg4 addServices:(BOOL)arg5;
+ (void)handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector *)arg2 event:(id)arg3 view:(id)arg4 windowController:(id)arg5 addPlugIns:(BOOL)arg6;
@end



@interface TDimmableIconImageView : NSImageView
{
}

- (void)drawRect:(struct CGRect)arg1;

@end

@interface TListRowView : NSTableRowView
{
    struct TFENode _node;
    TListViewController *_listViewController;
//    struct TNSRef<TListRowSelectionView *> _selectionView;
    _Bool _isDropTarget;
}

@property(nonatomic) _Bool isDropTarget; // @synthesize isDropTarget=_isDropTarget;
@property(nonatomic) TListViewController *listViewController; // @synthesize listViewController=_listViewController;
@property(nonatomic) struct TFENode node; // @synthesize node=_node;
- (void)openNode;
- (void)setSelected:(BOOL)arg1;
- (void)updateCellSelectedStateAppearance;
- (void)layout;
- (void)setNeedsLayout:(BOOL)arg1;
- (struct CGRect)selectionFrame;
- (void)updateLayer;
- (_Bool)isRowAfterSelected;
- (_Bool)isRowBeforeSelected;
- (long long)selectionHighlightStyle;
- (void)forceDisclosureTriangleBackgroundStyle;
- (id)disclosureTriangleButton;
- (void)dealloc;
- (id)initWithFrame:(struct CGRect)arg1;

@end
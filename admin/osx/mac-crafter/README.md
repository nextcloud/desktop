# mac-crafter

A tool to easily build a fully-functional Nextcloud Desktop Client for macOS.

It will create the application bundle with the option to sign it, which is required by Apple when running and debugging the file provider module.

## System requirements

- Mac OS > 11
- XCode
- Python

## Step by step usage

1. Clone the desktop client in your mac OS
```
https://github.com/nextcloud/desktop.git
```

2. Build the desktop client:
```
cd admin/osx/mac-crafter
swift run mac-crafter
```

3.1 Sign the application bundle by adding the following parameter:
`-c <Development or developer codesigning certificate name>`
The whole command will look like this:
```
swift run mac-crafter -c "Apple Development: <certificate common name>"
```

The resulting app bundle will be placed under the `product` folder in the mac-crafter folder.

> [!TIP] 
> Check [Apple's oficial documentation about code signing](https://developer.apple.com/documentation/xcode/using-the-latest-code-signature-format).

3.2 Build the file provider module with this option:
```
--build-file-provider-module
```

3.3 Disable the auto-updater:
```
--disable-autoupdater
```

3.4 Manually set the architecture you are building for:

```
--arch arm64
```

The options are `x86_64` and `arm64`.

### How to build the app bundle for arm and intel

To achieve this we are using [KDE Craft](https://community.kde.org/Craft).

1. After running the steps above, you will be able to use the `CraftMaster.py` script for that:

```
python build/craftmaster/CraftMaster.py --config <cloned desktop repo>/craftmaster.ini --target macos-clang-arm64 -c --add-blueprint-repository https://github.com/nextcloud/desktop-client-blueprints/
```

The `--target` options are define in the [`craftmaster.ini` file](https://github.com/nextcloud/desktop/blob/c771c4166c806d686d9b4f8b11e33d8d95631398/craftmaster.ini).

2. Install the client dependencies
```
python build/craftmaster/CraftMaster.py --config <cloned desktop repo>/craftmaster.ini --target macos-clang-arm64 -c --install-deps nextcloud-client
```

3. Build the client

```
python build/craftmaster/CraftMaster.py --config <cloned desktop repo>/craftmaster.ini --target macos-clang-arm64 -c --src-dir <cloned desktop repo> nextcloud-client
```

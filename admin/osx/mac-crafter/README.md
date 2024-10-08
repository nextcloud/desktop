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

3. Sign the application bundle by adding the following parameter:
`-c <Development or developer codesigning certificate name>`
The whole command will look like this:
```
swift run mac-crafter -c "Apple Development: <certificate common name>"
```

> [!NOTE] 
> The resulting app bundle will be placed under the `product` folder in the mac-crafter folder.

> [!TIP] 
> Check [Apple's oficial documentation about code signing](https://developer.apple.com/documentation/xcode/using-the-latest-code-signature-format).

### Extra options

- Build the file provider module with this option:
```
--build-file-provider-module
```

- Disable the auto-updater:
```
--disable-autoupdater
```

- Manually set the architecture you are building for:

```
--arch <arm64|x86_64>
```

### How to build the app bundle for arm and intel

To achieve this we are using a Python script called `make_universal.py` which bundles together the arm64 and Intel builds into one universal app bundle. This script can be found under `admin/osx`. You can invoke it like so:

```
python admin/osx/make_universal.py <x86 build path> <arm64 build path> <final target path>
```

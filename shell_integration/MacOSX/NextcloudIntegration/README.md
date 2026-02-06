# Nextcloud macOS Integration Project

This is an Xcode project to build platform-specific components for the Nextcloud desktop client.
As an example, this includes the file provider extension.

## Localization

Transifex is used for localization.
Currently, [the file provider extension](https://app.transifex.com/nextcloud/nextcloud/client-fileprovider/) and [file provider UI extension](https://app.transifex.com/nextcloud/nextcloud/client-fileproviderui/) both have a resource there.
These localizations are excluded from our usual and automated translation flow due to how Transifex synchronizes Xcode string catalogs and the danger of data loss.
To pull updated localizations from Transifex into the Xcode project manually, follow the steps below.

### Configuration

The dedicated [`.tx/config`](.tx/config) file is used.

## Pulling Translations

Run this in the "NextcloudIntegration" project folder of your repository clone:

```sh
tx pull --force --all --mode=translator
```

Do not commit the changes string catalogs, they need to be processed first.

### Sanitize Translations

Transifex returns empty strings for keys with untranslated localizations.
To remove them, we use the Swift command-line utility [TransifexStringCatalogSanitizer](../TransifexStringCatalogSanitizer/).
See its dedicated README for usage instructions.
Use it for all updated Xcode string catalogs.

### Summary

```sh
tx pull --force --all --mode=translator
swift run --package-path=../TransifexStringCatalogSanitizer TransifexStringCatalogSanitizer ./FileProviderExt/Localizable.xcstrings
swift run --package-path=../TransifexStringCatalogSanitizer TransifexStringCatalogSanitizer ./FileProviderUIExt/Localizable.xcstrings
```

## Pushing Translations

**Follow this section carefully to the end before performing any steps of it.**
The way Transifex handles Xcode string catalogs poses a high risk of accidentally deleting already available and finished translations on Transifex because pushing an Xcode string catalog overwrites the online state with the catalog as it is.
This means that changes on Transifex must be integrated locally first to avoid data loss, before the then updated local Xcode string catalog can be pushed to Transifex.

1. Perform the steps in the previous section about pulling translations.
2. Build the extensions in Xcode. This causes the compiler to update the string catalogs based on the current source code by automatically recognizing localizable texts. As of writing, the "desktopclient" scheme is a good choice because it builds both file provider extensions as dependencies.
3. Run the `TransifexStringCatalogSanitizer` over both string catalogs as in the previous section.
4. Inspect the changes in the string catalogs in a Git diff or whatever tool you use for that task. Verify the plausibility of each change.
5. Run `tx push` in the "NextcloudIntegration" directory.
6. Check Transifex to have received the new keys and deleted the obsolete ones.

## Nextcloud Developer Build

There is a special target in the Xcode project which integrates the `mac-crafter` command-line tool as an external build system in form of a scheme.
In other words: The "NextcloudDev" scheme enables you to build, run and debug the Nextcloud desktop client with the simple run action in Xcode. 

### Requirements

- You must have an Apple Development certificate for signing in your keychain.

### Usage

1. Copy [`Build.xcconfig.template`](NextcloudDev/Build.xcconfig.template) in the "NextcloudDev" source code folder to `Build.xcconfig` in the same location and adjust the values in it to your local setup.
2. Build or run the "NextcloudDev scheme".

### Known Issues

- Right now, the project does not support signing a different app identity than the default one (`com.nextcloud.desktopclient`) which is owned by the Nextcloud GmbH development team registered with Apple.
  This means that you have to be signed in with a developer account in Xcode which is part of that development team when building.
- Even when building successfully, Xcode may conclude that the build failed or at least some errors occurred during the build.
  During the build, some command outputs messages with an "Error: " prefix.
  Since this is the same way the compiler usually reports errors to Xcode, the latter assumes something might have gone wrong.
  But no invocation exits with an error code.
  Hence, the build can still complete successfully while Xcode might just misinterpret the console output. 
  You will see at the end of the build output log in Xcode.

### How It Works

- The "NextcloudDev" target runs the [Craft.sh](NextcloudDev/Craft.sh) shell script which is part of this Xcode project.
- `Craft.sh` prepares the execution of and finally runs [`mac-crafter`](https://github.com/nextcloud/desktop/tree/master/admin/osx/mac-crafter) which is part of the Nextcloud desktop client repository to simplify builds on macOS.
- By running `mac-crafter` with the right arguments and options, Xcode can attach to the built app with its debugger and stop at breakpoints.
  One of the key factors is the `Debug` build type which flips certain switches in the CMake build scripts ([in example: app hardening or get-task-allow entitlement](https://github.com/nextcloud/desktop/pull/8474/files)).
- The built Nextcloud desktop client app bundle is not placed into a derived data directory of Xcode but `/Applications`.
  The standard behavior of placing the product into Xcode's derived data directory would result in absolute reference paths within the scheme file which are not portable across devices and users due to varying user names.

### Hints

Just for reference, a few helpful snippets for inspecting state on breakpoints with the Xcode debugger.

#### Print a `QString`

```lldb
call someString.toStdString()
```

#### Print a `QStringList`

```lldb
call someStrings.join("\n").toStdString()
```

#### Attach to File Provider Extension Process

You can debug the file provider extension process(es) in Xcode by attaching to them by their binary name.

1. Select this menu item in Xcode: _Debug_ → _Attach to Process by PID or Name..._
2. Enter `FileProviderExt`.
   If you would also like to debug the file provider UI extension, then you can additionally specify `FileProviderUIExt`.
3. Confirm.
4. If no process is already running, then Xcode will wait for it to be launched to attach automatically.
   This usually happens when you launch the main app or set up a new account with file provider enabled.

#### Work on NextcloudFileProviderKit and NextcloudKit

You can directly debug changes to these dependencies edited from this project.
You have to have local repository clones of the packages somewhere locally, though.
Drag and drop the package folders into the project navigator of the NextcloudIntegration project.
This will cause Xcode to resolve to the local and editable package instead of a cached read-only clone from GitHub.
When you then run the build action of this root project, the local dependency is built as well.

# Nextcloud macOS Integration Project

This is an Xcode project to build platform-specific components for the Nextcloud desktop client.
As an example, this includes the file provider extension.

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

## Patches used

There are our patches on top of Qt 5.6.2, which we are currently
using for our binary packages on Windows and macOS. Most of them
have been sent upstream and are part of newer Qt releases.

All changes are designed to be upstream, and all those that are
special hacks to Qt will bear a NOUPSTREAM in their name

You can apply those patches on a git clone using:

```
git am <client>/admin/qt/patches/qtbase/*.patch
```

You can update them using:

```
git format-patch -N --no-signature -o <client>/admin/qt/patches/qtbase/ <v5.x.y>
```

# Shipping

This readme file is for distribution packagers and provides some reasoning and hints about shipping the desktop client for a seamless experience on the Linux desktop.

For that, ownCloud and the distributions need to collaborate.

Note that this information is valid for the ownCloud Desktop App 6.0 and subsequent releases only and does not apply for releases 5.x and below.
## Integration Problems

The ownCloud desktop client gets shipped as AppImage because it is effortless to create, independent from the underlying distro and easy to install, as long as the distribution is not providing a package on their own which is frequently updated.

With AppImage, there comes one problem: For the integration into the desktop file managers which are provided by the distribution (such as Dolphin for KDE and Nautilus), icons and partly also binary code is needed. That is needed to provide the neccessary information to enable the file managers to show for example icon overlays.

So far, the neccessary code and resources were bundled with the desktop client code, but now the desktop shell integrations resources were split off, so that they can be released and shipped separately.

## Solution

The idea is that everything that is needed to integrate into the desktop apps such as dolphin are part of the Linux distribution that any user chooses to use. Ideally they are packages maintained downstream in the Linux distributions and can be installed via the native package management.

Since these parts are very rarely changing, these packages are very stable and thus, packaging efforts a very very low.

The actual client code, which changes far more often, is either also provided by the Linux distro downstream or by ownCloud as an AppImage. ownCloud chooses to deliver an AppImage because that reduces the effort to package for all distros out there, it is "one fits all".

User now only have to install the AppImage with the latest client, and the shell integrations from their Linux distro package management. The AppImage based client automagically connects to the shell integration code and the overlay icons and menu additions in the file managers work.

With that, the user gets on the one hand very frequently updated desktop clients directly from the project, and nice integration with the desktop technology that is delivered from the distribution.

## Packaging Hints

Here are some hints for packager of the linux distributions which we kindly ask to change their packages in the distros to help us providing the best ownCloud client integration experience:

The shell integrations are available for [KDE Dolphin](https://github.com/owncloud/client-desktop-shell-integration-dolphin), [Nautilus and Caja](https://github.com/owncloud/client-desktop-shell-integration-nautilus). While KDE Dolphin needs compiled code, the latter two are using python.
Each of them should be available as separate distro package, so that they can be suggested to complement "their" file manager.

All of these packages should depend on the [client-extension-resources](https://github.com/owncloud/client-desktop-shell-integration-resources) package, which basically only contains the overlay icons. They can optionally be branded with a distro specific theme, to maintain a cool desktop experience.

Non of the three file manager integration packages (for Dolphin, Nautilus or Caja) need a dependency on the "big" ownCloud desktop client package any more, so that they do not change in foreseeable times.

If the distro decides to ship an own package of the client or lets their user install the provided AppImage, the experience will be perfect in any case.


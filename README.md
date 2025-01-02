# Nextcloud Desktop Client

The Nextcloud Desktop Client is a tool to synchronize files from Nextcloud Server with your computer.

<p align="center">
    <img src="doc/images/main_dialog_christine.png" alt="Desktop Client on Windows" width="450">
</p>

## :rocket: Releases
For the latest stable recommended version, please refer to the [download page https://nextcloud.com/install/#install-clients](https://nextcloud.com/install/#install-clients)

## Contributing to the desktop client
:v: Please read the [Code of Conduct](https://nextcloud.com/community/code-of-conduct/). This document offers some guidance to ensure Nextcloud participants can cooperate effectively in a positive and inspiring atmosphere and to explain how together we can strengthen and support each other.

### 👪 Join the team
There are many ways to contribute, of which development is only one! Find out [how to get involved](https://nextcloud.com/contribute/), including as a translator, designer, tester, helping others, and much more! 😍

### Help testing
Download and install the client:<br>
[🔽 All releases](https://github.com/nextcloud-releases/desktop/releases)<br>
[🔽 Daily master builds](https://download.nextcloud.com/desktop/daily)

### Reporting issues
If you find any bugs or have any suggestion for improvement, please
[open an issue in this repository](https://github.com/nextcloud/desktop/issues).

### Bug fixing and development
#### 1. 🚀 Set up your local development environment

1.1 System requirements
- [Windows 10, Windows 11]((https://github.com/nextcloud/desktop-client-blueprints/)), Mac OS > 10.14 or Linux

> [!NOTE]  
> Find the system requirements and instructions on [how to work on Windows with KDE Craft](https://github.com/nextcloud/desktop-client-blueprints/) on our [desktop client blueprints repository](https://github.com/nextcloud/desktop-client-blueprints/).

- [🔽 Inkscape (to generate icons)](https://inkscape.org/release/)
- Developer tools: cmake, clang/gcc/g++:
- Qt6 since 3.14, Qt5 for earlier versions
- OpenSSL
- [🔽 QtKeychain](https://github.com/frankosterfeld/qtkeychain)
- SQLite

1.2 Optional
- [Qt Creator IDE](https://www.qt.io/product/development-tools)
- [delta: A viewer for git and diff output](https://github.com/dandavison/delta)

> [!TIP]
> We highly recommend [Nextcloud development environment on Docker Compose](https://juliusknorr.github.io/nextcloud-docker-dev/) for testing/bug fixing/development.<br>
> ▶️ https://juliusknorr.github.io/nextcloud-docker-dev/

1.3 Step by step instructions on how to build the client to contribute
1. Clone the Github repository:
```
git clone https://github.com/nextcloud/desktop.git
```
2. Create <build directory>:
```
mkdir <build directory>
```
3. Compile:
```
cd <build directory>
cmake -S <cloned desktop repo> -B build -DCMAKE_INSTALL_PREFIX=<dependencies> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=. -DNEXTCLOUD_DEV=ON
```

> [!TIP]
> The cmake variabel NEXTCLOUD_DEV allows you to run your own build of the client while developing in parallel with an installed version of the client.

4. Build it:
- Windows:
```
cmake --build .
```
- Other platforms:
```
make
```

> [!TIP]
> For building the client for mac OS we have a tool called mac-crafter.
> You will find instructions on how to use it at [admin/osx/mac-crafter](https://github.com/nextcloud/desktop/tree/32305e4c15ff95d00fae07e82e750fe9051b2250/admin/osx/mac-crafter).

5. 🐛 [Pick a good first issue](https://github.com/nextcloud/desktop/labels/good%20first%20issue)
6. 👩‍🔧 Create a branch and make your changes. Remember to sign off your commits using `git commit -sm "Your commit message"`
7. ⬆ Create a [pull request](https://opensource.guide/how-to-contribute/#opening-a-pull-request) and `@mention` the people from the issue to review
8. 👍 Fix things that come up during a review
9. 🎉 Wait for it to get merged!

## Get in touch 💬
* [📋 Forum](https://help.nextcloud.com)
* [👥 Facebook](https://www.facebook.com/nextclouders)
* [🐣 Twitter](https://twitter.com/Nextclouders)
* [🐘 Mastodon](https://mastodon.xyz/@nextcloud)

You can also [get support for Nextcloud](https://nextcloud.com/support)!

## :scroll: License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.

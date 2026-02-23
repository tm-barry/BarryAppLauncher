<p align="center">
	<img alt="logo" src="assets/icons/barryapplauncher.svg" width="256" height="256">
</p>

<h1 align="center">BarryAppLauncher</h1>

<p align="center">
	BarryAppLauncher is a lightweight Qt app that seamlessly integrates AppImages into your desktop menu and lets you update them in-place.
</p>

## App List / App Info

Quickly view/update all registered AppImages. View info, unlock, register, and update single AppImages.

| App List | App Info |
|:--------:|:--------:|
| ![App List](assets/images/barryapplauncher-main.png) | ![App Info](assets/images/barryapplauncher-appinfo.png) |

## AppImage Updater

Supports updating AppImages that have releases exposed through a JSON API or static download link.

| JSON Updater | Static Updater |
|:------------:|:--------------:|
| ![JSON Updater](assets/images/barryapplauncher-appinfo-update-json.png) | ![Static Updater](assets/images/barryapplauncher-appinfo-update-static.png) |

## Preferences

### General
- Allows users to specify where registered AppImages are stored.
- Specify whether to move or copy AppImages when registering.
- BarryAppLauncher can register itself into the desktop menu.

### Updater
- Keep a backup of the previous version when updating AppImages.
- Allows setting headers used when updating. Useful for setting authorization headers for web APIs.

| General Preferences | Updater Preferences |
|:-----------------:|:-----------------:|
| ![General Preferences](assets/images/barryapplauncher-preferences-general.png) | ![Updater Preferences](assets/images/barryapplauncher-preferences-updater.png) |

## Build

```bash
# Build Dockerfile
docker build -t barryapplauncher-builder .

# Build AppImage
docker run --rm -it \
    -u $(id -u):$(id -g) \
    -v "$(pwd):/home/user/project" \
    barryapplauncher-builder \
    bash -c "\
      cd /home/user/project && \
      qt-cmake . -G Ninja -B build/AppImage-Release -DCMAKE_BUILD_TYPE=Release && \
      cmake --build build/AppImage-Release && \
      ./build_appimage.sh /home/user/project"
```
## License

BarryAppLauncher is licensed under the MIT license.

This software includes **libarchive** (BSD-2-Clause), which is used for ZIP file handling.

[Jump to license](LICENSE)

If you distribute the AppImage, the included LICENSE file contains the full licenses for BarryAppLauncher and all third-party components, including libarchive.


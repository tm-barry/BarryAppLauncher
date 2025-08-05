<p align="center">
	<img alt="logo" src="assets/icons/barryapplauncher.svg" width="256" height="256">
</p>

<h1 align="center">BarryAppLauncher</h1>

BarryAppLauncher is a lightweight Qt app for integrating AppImages into your desktop menu.

## Build

```
# Build Dockerfile
docker build -t barryapplauncher-builder .

# Build AppImage
docker run --rm -it \
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

[Jump to license](license.md)

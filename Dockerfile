FROM stateoftheartio/qt6:6.8-gcc-aqt

USER root

# Install OpenGL, Vulkan, and AppImage extract dependencies
RUN mkdir -p /var/lib/apt/lists/partial && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        libgl-dev \
        libvulkan-dev \
        libegl1-mesa-dev \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        rsync \
        file \
        binutils \
        wget \
        libfuse2 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Remove old versions
RUN rm -f /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-qt

# Download and extract AppImages
RUN mkdir -p /opt/linuxdeploy && \
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -O /opt/linuxdeploy/linuxdeploy.AppImage && \
    chmod +x /opt/linuxdeploy/linuxdeploy.AppImage && \
    /opt/linuxdeploy/linuxdeploy.AppImage --appimage-extract && \
    mv squashfs-root /opt/linuxdeploy/linuxdeploy-extracted && \
    ln -s /opt/linuxdeploy/linuxdeploy-extracted/AppRun /usr/local/bin/linuxdeploy && \
    \
    wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -O /opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage && \
    chmod +x /opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage && \
    /opt/linuxdeploy/linuxdeploy-plugin-qt.AppImage --appimage-extract && \
    mv squashfs-root /opt/linuxdeploy/linuxdeploy-plugin-qt-extracted && \
    ln -s /opt/linuxdeploy/linuxdeploy-plugin-qt-extracted/AppRun /usr/local/bin/linuxdeploy-plugin-qt

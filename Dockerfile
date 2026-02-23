FROM ubuntu:22.04

# Arguments
ARG AQT_VERSION="aqtinstall==3.3.0"
ARG QT_VERSION=6.10.2
ARG LINUXDEPLOY_VERSION=continuous
ARG LINUXDEPLOY_PLUGIN_QT_VERSION=continuous
ARG QT_PATH=/opt/Qt
ARG ADDITIONAL_PACKAGES="sudo git openssh-client ca-certificates build-essential curl python3 locales patchelf \
    libgl-dev libvulkan-dev libegl1-mesa-dev libx11-dev libxext-dev \
    libxrandr-dev libxkbcommon-dev rsync file binutils wget libfuse2"

# Environment
ENV DEBIAN_FRONTEND=noninteractive \
    QT_VERSION=${QT_VERSION} \
    AQT_VERSION=${AQT_VERSION} \
    QT_PATH=${QT_PATH} \
    QT_GCC=${QT_PATH}/${QT_VERSION}/gcc_64 \
    PATH=${QT_PATH}/Tools/CMake/bin:${QT_PATH}/Tools/Ninja:${QT_PATH}/${QT_VERSION}/gcc_64/bin:$PATH

# Install Qt
RUN set -eux; \
    # Save original packages
    dpkg --get-selections | cut -f1 > /tmp/packages_orig.lst; \
    \
    # Minimal deps for Qt installation
    apt-get update; \
    apt-get install -y --no-install-recommends git python3-pip libglib2.0-0; \
    \
    # Install aqtinstall
    pip3 install --no-cache-dir "${AQT_VERSION}"; \
    \
    # Install Qt and tools
    aqt install-qt -O "${QT_PATH}" linux desktop "${QT_VERSION}" linux_gcc_64; \
    aqt install-tool -O "${QT_PATH}" linux desktop tools_cmake; \
    aqt install-tool -O "${QT_PATH}" linux desktop tools_ninja; \
    \
    # Remove pip packages to keep image small
    pip3 freeze | xargs pip3 uninstall -y; \
    \
    # Restore original packages
    dpkg --get-selections | cut -f1 > /tmp/packages_curr.lst; \
    grep -Fxv -f /tmp/packages_orig.lst /tmp/packages_curr.lst | xargs apt-get remove -y --purge; \
    \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*

# Install linuxdeploy and extract AppImages
RUN set -eux; \
    apt-get update && apt-get install -y --no-install-recommends ${ADDITIONAL_PACKAGES}; \
    mkdir -p /opt/linuxdeploy /usr/local/bin; \
    cd /opt/linuxdeploy; \
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-x86_64.AppImage -O linuxdeploy.AppImage; \
    chmod +x linuxdeploy.AppImage; \
    ./linuxdeploy.AppImage --appimage-extract; \
    mv squashfs-root linuxdeploy-extracted; \
    ln -sf /opt/linuxdeploy/linuxdeploy-extracted/AppRun /usr/local/bin/linuxdeploy; \
    wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_PLUGIN_QT_VERSION}/linuxdeploy-plugin-qt-x86_64.AppImage -O linuxdeploy-plugin-qt.AppImage; \
    chmod +x linuxdeploy-plugin-qt.AppImage; \
    ./linuxdeploy-plugin-qt.AppImage --appimage-extract; \
    mv squashfs-root linuxdeploy-plugin-qt-extracted; \
    ln -sf /opt/linuxdeploy/linuxdeploy-plugin-qt-extracted/AppRun /usr/local/bin/linuxdeploy-plugin-qt; \
    apt-get clean; rm -rf /var/lib/apt/lists/* /opt/linuxdeploy/*.AppImage

# Find and install missing runtime libs
RUN set -eux; \
    QT_BIN_PATH="${QT_PATH}/${QT_VERSION}/gcc_64"; \
    MISSING_LIBS=$(find "$QT_BIN_PATH" /usr/local -executable -type f -o -name '*.so' | xargs ldd 2>/dev/null \
                    | grep '=> not found' | awk '{print $1}' | sort -u); \
    if [ -n "$MISSING_LIBS" ]; then \
        apt-get update; \
        apt-get install -y apt-file; \
        apt-file update; \
        for lib in $MISSING_LIBS; do \
            PKG=$(apt-file find "$lib" | grep '^lib' | head -n1 | cut -d: -f1); \
            if [ -n "$PKG" ]; then \
                apt-get install -y --no-install-recommends "$PKG"; \
            fi; \
        done; \
        apt-get autoremove -y --purge apt-file; \
        apt-get clean; \
        rm -rf /var/lib/apt/lists/*; \
    fi

# Reconfigure locale
RUN locale-gen en_US.UTF-8 && dpkg-reconfigure locales

# Add user
RUN groupadd -r user && useradd --create-home --gid user user && echo 'user ALL=NOPASSWD: ALL' > /etc/sudoers.d/user

USER user
WORKDIR /home/user
ENV HOME /home/user

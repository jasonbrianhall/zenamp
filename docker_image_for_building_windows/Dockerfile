FROM registry.fedoraproject.org/fedora:43

RUN dnf -y update

# Enable RPM Fusion repositories
RUN dnf -y install \
         https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
         https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

# Some GCC Tasks
RUN dnf -y install \
        # Core build tools
        make gcc gcc-c++ pkgconfig \
        autoconf automake libtool \
        cmake ninja-build \
        git which findutils \
        zip unzip xxd

RUN dnf -y install gtk4-devel glib2-devel pango-devel gdk-pixbuf2-devel \
        SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
        openssl-devel taglib-devel \
        gcc gcc-c++ make \
        gtk3-devel gtkmm30-devel \
        SDL2-devel SDL2_mixer-devel \
        libvorbis-devel flac-devel \
        gstreamer1-devel gstreamer1-plugins-base-devel \
        pkg-config \
        wine wine-devel \
        zip unzip \
        opus-devel opusfile-devel \
        ImageMagick \
        ffmpeg-devel \
        taglib-devel \
        rpm-build rpmdevtools \
        meson

RUN dnf -y install \
        glew-devel freetype-devel libssh2-devel

RUN dnf -y install \
        # MinGW cross toolchain
        mingw64-gcc mingw64-gcc-c++ mingw64-binutils \
        mingw64-crt mingw64-headers \
        mingw64-gtk4 \
        mingw64-SDL2 mingw64-SDL2_image mingw64-SDL2_ttf mingw64-SDL2_mixer \
        bash \
        mingw64-taglib \
        mingw64-openssl \
        mingw64-gtk3 mingw64-gtkmm30 \
        mingw64-SDL2 mingw64-SDL2_mixer \
        mingw64-libvorbis mingw64-flac \
        mingw64-opus mingw64-opusfile \
        mingw64-taglib.noarch \
        mingw64-glew mingw64-gdk-pixbuf \
        mingw-binutils-generic \
        mingw-filesystem-base \
        mingw64-filesystem \
        mingw64-crt \
        mingw64-zlib \
        mingw64-win-iconv \
        mingw64-libxml2 \
        mingw64-winpthreads \
        mingw64-libgcc \
        mingw64-libjpeg-turbo \
        mingw64-vkd3d \
        mingw64-libstdc++ \
        mingw64-libtiff \
        mingw64-libxslt \
        mingw64-libpng \
        mingw64-lcms2 \
        mingw64-SDL3 \
        mingw64-FAudio \
        mingw32-wine-gecko \
        mingw64-wine-gecko \
        mingw64-libogg \
        mingw64-libsigc++20 \
        mingw64-openssl \
        mingw64-sdl2-compat \
        mingw64-fribidi \
        mingw64-libidn2 \
        mingw64-orc \
        mingw64-imath \
        mingw64-bzip2 \
        mingw64-freetype \
        mingw64-angleproject \
        mingw64-libepoxy \
        mingw64-adwaita-icon-theme \
        mingw64-opus \
        mingw64-libssh2 \
        mingw64-libvorbis \
        mingw64-libtheora \
        mingw64-libdeflate \
        mingw64-openexr \
        mingw64-gsm \
        mingw64-directx-headers \
        mingw64-expat \
        mingw64-fontconfig \
        mingw64-libunistring \
        mingw64-libpsl \
        mingw64-curl \
        mingw64-gmp \
        mingw64-nettle \
        mingw64-openjpeg \
        mingw64-openal-soft \
        mingw64-directxmath \
        mingw64-giflib \
        mingw64-libwebp \
        mingw64-icu \
        mingw64-pcre2 \
        mingw64-libffi \
        mingw64-binutils \
        mingw64-termcap \
        mingw64-gettext \
        mingw64-glib2 \
        mingw64-gdk-pixbuf \
        mingw64-glibmm24 \
        mingw64-gstreamer1 \
        mingw64-atk \
        mingw64-atkmm \
        mingw64-gtk4-update-icon-cache \
        mingw64-gtk-update-icon-cache \
        mingw64-graphene \
        mingw64-pixman \
        mingw64-cairo \
        mingw64-harfbuzz \
        mingw64-pango \
        mingw64-librsvg2 \
        mingw64-gstreamer1-plugins-base \
        mingw64-cairomm \
        mingw64-pangomm \
        mingw64-gstreamer1-plugins-bad-free \
        mingw64-gtk3 \
        mingw64-winpthreads-static \
        mingw64-cpp \
        mingw64-headers \
        mingw64-gcc \
        mingw64-gcc-c++ \
        mingw64-gtkmm30 \
        mingw64-gtk4 \
        mingw64-SDL2_ttf \
        mingw64-SDL2_image \
        mingw64-opusfile \
        mingw64-SDL2_mixer \
        mingw64-flac \
        mingw64-glew \
        mingw64-taglib


RUN dnf -y install \
       rsync

RUN dnf -y install gcc gcc-c++ make \
          SDL2-devel \
          freetype-devel \
          glew-devel \
          pkg-config \
          mingw64-gcc mingw64-gcc-c++ \
          mingw64-SDL2 \
          mingw64-freetype \
          mingw64-glew \
          mingw64-libssh2 \
          mingw64-gtk3 mingw64-gtkmm30 \
          mingw64-SDL2 mingw64-SDL2_mixer \
          mingw64-libvorbis mingw64-flac \
          mingw64-opus mingw64-opusfile \
          mingw64-taglib.noarch \
          mingw64-glew \
          mingw64-gmp \
          mingw64-gstreamer1 \
          mingw64-gstreamer1-plugins-base \
          wine \
          zip unzip \
          ImageMagick \
          libssh2-devel \
          libwebp-devel \
          mingw64-libwebp \
          mingw64-wxWidgets3 \
          xxd

RUN dnf clean all

# Rootless podman user
RUN useradd -u 1000 builder && \
    mkdir -p /home/builder/.local/share/containers && \
    chown -R builder:builder /home/builder

ENV _CONTAINERS_USERNS_CONFIGURED=1

USER builder
WORKDIR /home/builder

CMD ["/bin/bash"]


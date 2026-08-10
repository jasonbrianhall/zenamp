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
        zip unzip

        # Podman stack
RUN dnf -y install  podman buildah skopeo fuse-overlayfs shadow-utils
        # Native GTK4/SDL2 (for pkg-config introspection)

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
        rpm-build rpmdevtools


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
        mingw64-taglib.noarch && \
    dnf clean all

# Rootless podman user
RUN useradd -u 1000 builder && \
    mkdir -p /home/builder/.local/share/containers && \
    chown -R builder:builder /home/builder

ENV _CONTAINERS_USERNS_CONFIGURED=1

USER builder
WORKDIR /home/builder

CMD ["/bin/bash"]


FROM registry.fedoraproject.org/fedora:43

RUN dnf -y update

# Separate Tasks 
RUN dnf -y install \
        # Core build tools
        make gcc gcc-c++ pkgconfig \
        autoconf automake libtool \
        cmake ninja-build \
        git which findutils \
        zip unzip \
        # Podman stack
        podman buildah skopeo fuse-overlayfs shadow-utils \
        # Native GTK4/SDL2 (for pkg-config introspection)
        gtk4-devel glib2-devel pango-devel gdk-pixbuf2-devel \
        SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel \
        # MinGW cross toolchain
        mingw64-gcc mingw64-gcc-c++ mingw64-binutils \
        mingw64-crt mingw64-headers \
        mingw64-gtk4 \
        mingw64-SDL2 mingw64-SDL2_image mingw64-SDL2_ttf mingw64-SDL2_mixer \
        bash \
        mingw64-taglib && \
    dnf clean all

# Rootless podman user
RUN useradd -u 1000 builder && \
    mkdir -p /home/builder/.local/share/containers && \
    chown -R builder:builder /home/builder

ENV _CONTAINERS_USERNS_CONFIGURED=1

USER builder
WORKDIR /home/builder

CMD ["/bin/bash"]


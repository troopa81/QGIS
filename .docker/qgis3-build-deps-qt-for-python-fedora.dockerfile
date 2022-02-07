
FROM fedora:33
MAINTAINER Matthias Kuhn <matthias@opengis.ch>

RUN dnf -y install \
    bison \
    clang \
    clazy \
    exiv2-devel \
    fcgi-devel \
    flex \
    gdal-devel \
    geos-devel \
    gsl-devel \
    libpq-devel \
    libspatialite-devel \
    libzip-devel \
    ninja-build \
    proj-devel \
    protobuf-devel \
    protobuf-lite-devel \
    qca-qt5-devel \
    qscintilla-qt5-devel \
    qt5-qt3d-devel \
    qt5-qtbase-devel \
    qt5-qtlocation-devel \
    qt5-qtserialport-devel \
    qt5-qttools-static \
    qt5-qtwebkit-devel \
    qtkeychain-qt5-devel \
    qwt-qt5-devel \
    spatialindex-devel \
    sqlite-devel \
    unzip \
    python3-shiboken2 \
    python3-shiboken2-devel \
    python3-pyside2 \
    python3-pyside2-devel \
    pyside2-tools \
    xorg-x11-server-Xvfb

RUN dnf -y install ccache python3-devel

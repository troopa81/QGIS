ARG DISTRO_VERSION=40

FROM fedora:${DISTRO_VERSION} as single
MAINTAINER Matthias Kuhn <matthias@opengis.ch>

RUN dnf -y --refresh install \
    bison \
    ccache \
    clang \
    clazy \
    exiv2-devel \
    expat-devel \
    fcgi-devel \
    flex \
    git \
    gdal-devel \
    geos-devel \
    gpsbabel \
    grass \
    grass-devel \
    gsl-devel \
    libpq-devel \
    libspatialite-devel \
    libxml2-devel \
    libxslt-devel \
    libzip-devel \
    libzstd-devel \
    llvm-devel \
    netcdf-devel \
    ninja-build \
    ocl-icd-devel \
    PDAL \
    PDAL-libs \
    PDAL-devel \
    proj-devel \
    protobuf-devel \
    protobuf-lite-devel \
    python3-devel \
    python3-gdal \
    python3-termcolor \
    qt6-qt3d-devel \
    qt6-qtbase-devel \
    qt6-qtdeclarative-devel \
    qt6-qttools-static \
    qt6-qtsvg-devel \
    qt6-qtpositioning-devel \
    qt6-qtdeclarative-devel \
    qt6-qt5compat-devel \
    spatialindex-devel \
    sqlite-devel \
    unzip \
    unixODBC-devel \
    xorg-x11-server-Xvfb \
    util-linux \
    wget \
    openssl-devel \
    libsecret-devel \
    make \
    automake \
    ninja-build \
    patch \
    dos2unix

RUN dnf -y --refresh install \
    qca-qt6-devel \
    qtkeychain-qt6-devel \
    qwt-qt6-devel \
    qscintilla-qt6-devel

RUN dnf -y --refresh install clang-devel llvm-devel

RUN dnf -y --refresh install python3-pyside6-devel python3-shiboken6-devel python3-pyside6-devel

ENV PATH="/usr/local/bin:${PATH}"

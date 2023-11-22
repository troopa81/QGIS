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

RUN dnf -y --refresh install python3-pyside6-devel

# RUN dnf -y --refresh install python3-packaging

ENV PATH="/usr/local/bin:${PATH}"

# Doesn't work this way because Qt_6.6_PRIVATE_API differs between Qt pip/fedora package
# RUN dnf install -y python3-pip && pip install PySide6 shiboken6_generator

# # Ugly hack because shiboken6_generator link on an old icu librairy
# RUN ln -s /usr/lib64/libicui18n.so.72 /usr/lib64/libicui18n.so.56 \
#     && ln -s /usr/lib64/libicuuc.so.72 /usr/lib64/libicuuc.so.56 \
#     && ln -s /usr/lib64/libicudata.so.72 /usr/lib64/libicudata.so.56

# RUN dnf -y install patchelf qt6-qtbase-static qt6-qtbase-private-devel

# RUN git clone https://code.qt.io/pyside/pyside-setup && cd pyside-setup && git checkout v6.6.2 && mkdir /usr/modules \
#     && python3 setup.py install --qtpaths=/usr/bin/qtpaths6 --parallel=8

# RUN pip install shiboken6==6.6.2 pyside6==6.6.2 shiboken6_generator==6.6.2

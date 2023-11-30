ARG DISTRO_VERSION=37

FROM fedora:${DISTRO_VERSION} as single
MAINTAINER Matthias Kuhn <matthias@opengis.ch>

RUN dnf -y --refresh install \
    bison \
    ccache \
    clang \
    clang-devel \
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
    gcc \
    gcc-c++ \
    kernel-devel \
    ninja-build \
    patch \
    dos2unix

RUN cd /usr/src \
  && wget https://github.com/KDE/qca/archive/refs/heads/master.zip \
  && unzip master.zip \
  && rm master.zip \
  && mkdir build \
  && cd build \
  && cmake -DQT6=ON -DBUILD_TESTS=OFF -GNinja -DCMAKE_INSTALL_PREFIX=/usr/local ../qca-master \
  && ninja install

RUN cd /usr/src \
  && wget https://github.com/frankosterfeld/qtkeychain/archive/refs/heads/main.zip \
  && unzip main.zip \
  && rm main.zip \
  && cd qtkeychain-main \
  && cmake -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF -DCMAKE_INSTALL_PREFIX=/usr/local -GNinja \
  && ninja install

RUN cd /usr/src \
  && wget https://sourceforge.net/projects/qwt/files/qwt/6.2.0/qwt-6.2.0.zip/download \
  && unzip download \
  && cd qwt-6.2.0 \
  && dos2unix qwtconfig.pri \
  && printf '140c140\n< QWT_CONFIG     += QwtExamples\n---\n> #QWT_CONFIG     += QwtExamples\n151c151\n< QWT_CONFIG     += QwtPlayground\n---\n> #QWT_CONFIG     += QwtPlayground\n158c158\n< QWT_CONFIG     += QwtTests\n---\n> #QWT_CONFIG     += QwtTests\n' | patch qwtconfig.pri \
  && qmake6 qwt.pro \
  && make -j4 \
  && make install


RUN cd /usr/src \
  && wget https://www.riverbankcomputing.com/static/Downloads/QScintilla/2.13.3/QScintilla_src-2.13.3.zip \
  && unzip QScintilla_src-2.13.3.zip \
  && rm QScintilla_src-2.13.3.zip \
  && cd QScintilla_src-2.13.3 \
  && qmake6 src/qscintilla.pro \
  && make -j4 \
  && make install

ENV PATH="/usr/local/bin:${PATH}"

# Doesn't work this way because Qt_6.6_PRIVATE_API differs between Qt pip/fedora package
# RUN dnf install -y python3-pip && pip install PySide6 shiboken6_generator

# # Ugly hack because shiboken6_generator link on an old icu librairy
# RUN ln -s /usr/lib64/libicui18n.so.72 /usr/lib64/libicui18n.so.56 \
#     && ln -s /usr/lib64/libicuuc.so.72 /usr/lib64/libicuuc.so.56 \
#     && ln -s /usr/lib64/libicudata.so.72 /usr/lib64/libicudata.so.56

RUN dnf -y install patchelf qt6-qtbase-static qt6-qtbase-private-devel \
    && git clone https://code.qt.io/pyside/pyside-setup && cd pyside-setup && git checkout v6.5.1 && mkdir /usr/modules \
    && python3 setup.py install --qtpaths=/usr/bin/qtpaths6 --parallel=8

ARG DISTRO_VERSION=37

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
    libzip-devel \
    libzstd-devel \
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
  && wget https://github.com/frankosterfeld/qtkeychain/archive/refs/heads/master.zip \
  && unzip master.zip \
  && rm master.zip \
  && cd qtkeychain-master \
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

#RUN pip install --index-url=http://download.qt.io/official_releases/QtForPython/ --trusted-host download.qt.io shiboken6 pyside6 shiboken6_generator

ENV PATH="/usr/local/bin:${PATH}"

RUN dnf install -y libxslt-devel

RUN dnf -y install git python-setuptools python3-packaging clang-devel llvm-devel
WORKDIR /usr
#RUN git clone https://code.qt.io/pyside/pyside-setup && cd pyside-setup && git checkout 6.3
#RUN wget https://codereview.qt-project.org/changes/pyside%2Fpyside-setup~395307/revisions/1/archive?format=tar
#RUN mv archive?format=tar pyside.tar.gz && tar tzf pyside
#WORKDIR /usr/pyside-setup
#RUN python3 setup.py build --qtpaths=/usr/lib64/qt6/bin/qtpaths --parallel=8

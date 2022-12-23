
ARG DISTRO_VERSION=20.04

FROM      ubuntu:${DISTRO_VERSION}
MAINTAINER Denis Rouzaud <denis@opengis.ch>

LABEL Description="Docker container with QGIS dependencies" Vendor="QGIS.org" Version="1.0"


RUN apt-get update
RUN apt-get install -y software-properties-common
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cmake \
    curl \
    git \
    gdal-bin \
    gpsbabel \
    graphviz \
    libaio1 \
    libexiv2-27 \
    libfcgi0ldbl \
    libspatialindex6 \
    libsqlite3-mod-spatialite \
    lighttpd \
    locales \
    poppler-utils \
    python3-future \
    python3-gdal \
    python3-mock \
    python3-nose2 \
    python3-owslib \
    python3-pip \
    python3-pyproj \
    python3-sip \
    python3-termcolor \
    python3-yaml \
    supervisor \
    unzip \
    xauth \
    xvfb \
    bison \
    ccache \
    clang \
    flex \
    libexiv2-dev \
    libexpat1-dev \
    libfcgi-dev \
    libgdal-dev \
    libgeos-dev \
    libgsl-dev \
    libpdal-dev \
    libpq-dev \
    libproj-dev \
    libprotobuf-dev \
    libspatialindex-dev \
    libspatialite-dev \
    libsqlite3-dev \
    libsqlite3-mod-spatialite \
    libzip-dev \
    libzstd-dev \
    ninja-build \
    protobuf-compiler \
    python3-dev \
    opencl-headers

RUN pip3 install \
    numpy \
    nose2 \
    pyyaml \
    mock \
    future \
    termcolor \
    oauthlib \
    pyopenssl \
    pep8 \
    pexpect \
    capturer \
    sphinx \
    requests \
    six \
    hdbcli


# Avoid sqlcmd termination due to locale -- see https://github.com/Microsoft/mssql-docker/issues/163
RUN echo "nb_NO.UTF-8 UTF-8" > /etc/locale.gen
RUN echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen
RUN locale-gen

RUN echo "alias python=python3" >> ~/.bash_aliases


ENV PATH="/usr/local/bin:${PATH}"


RUN pip install aqtinstall

ENV QT_VERSION='6.4.1'
ENV QT_PATH="/opt/Qt"

RUN aqt install-qt --outputdir $QT_PATH linux desktop $QT_VERSION -m qtpositioning qtcharts qt5compat qtimageformats qtnetworkauth

ENV Qt6_DIR=${QT_PATH}/${QT_VERSION}/gcc_64/

RUN apt-get install -y wget

RUN cd /usr/src \
  && wget https://github.com/KDE/qca/archive/refs/heads/master.zip \
  && unzip master.zip \
  && rm master.zip \
  && mkdir build \
  && cd build \
  && cmake -DQT6=ON -DBUILD_TESTS=OFF -GNinja -DCMAKE_INSTALL_PREFIX=/usr/local ../qca-master \
  && ninja install

RUN apt-get install -y libclang-dev libxslt-dev llvm libgl1-mesa-dev libxkbcommon-dev

WORKDIR /usr/
RUN cd /usr && git clone https://code.qt.io/pyside/pyside-setup && cd pyside-setup && git checkout ${QT_VERSION}
WORKDIR /usr/pyside-setup
RUN mkdir -p /usr/pyside-setup/build/qfp-py3.10-qt6.4.1-64bit-release/install/lib/python3.10 && \
    cd       /usr/pyside-setup/build/qfp-py3.10-qt6.4.1-64bit-release/install/lib/python3.10 && \
    ln -s dist-packages site-packages
#RUN python3 setup.py build --qtpaths=${Qt6_DIR}/bin/qtpaths --parallel=8 --internal-build-type=shiboken6
#RUN python3 setup.py build --qtpaths=${Qt6_DIR}/bin/qtpaths --parallel=8 --internal-build-type=shiboken6-generator
RUN pip install -r requirements.txt
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt-get update
RUN apt-get install -y cmake

RUN python3 setup.py build --qtpaths=${Qt6_DIR}/bin/qtpaths --parallel=8
RUN python3 setup.py install --qtpaths=${Qt6_DIR}/bin/qtpaths --parallel=8 --verbose-build


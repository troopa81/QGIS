
ARG DISTRO_VERSION=22.04

# Oracle Docker image is too large, so we add as less dependencies as possible
# so there is enough space on GitHub runner
FROM      ubuntu:${DISTRO_VERSION}
MAINTAINER Denis Rouzaud <denis@opengis.ch>

LABEL Description="Docker container with QGIS dependencies" Vendor="QGIS.org" Version="1.0"

# && echo "deb http://ppa.launchpad.net/ubuntugis/ubuntugis-unstable/ubuntu xenial main" >> /etc/apt/sources.list \
# && echo "deb-src http://ppa.launchpad.net/ubuntugis/ubuntugis-unstable/ubuntu xenial main" >> /etc/apt/sources.list \
# && apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 314DF160 \

RUN apt-get update
RUN apt-get install -y software-properties-common
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
    apt-transport-https \
    ca-certificates \
    cmake \
    curl \
    dh-python \
    git \
    gdal-bin \
    gpsbabel \
    graphviz \
    libaio1 \
    libexiv2-27 \
    libfcgi0ldbl \
    libgsl27 \
    libprotobuf-lite23 \
    libqt6concurrent6 \
    libqt6positioning6 \
    libqt6qml6 \
    libqt6quick6 \
    libqt6quickcontrols2-6 \
    libqt6quickwidgets6 \
    libqt6serialport6 \
    libqt6sql6-sqlite \
    libqt6xml6 \
    libspatialindex6 \
    libsqlite3-mod-spatialite \
    lighttpd \
    locales \
    pdal \
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
    qpdf \
    qt6-image-formats-plugins \
    supervisor \
    unzip \
    xauth \
    xfonts-100dpi \
    xfonts-75dpi \
    xfonts-base \
    xfonts-scalable \
    xvfb \
    ocl-icd-libopencl1 \
    bison \
    ccache \
    clang \
    cmake \
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
    libqt6opengl6-dev \
    libqt6svg6-dev \
    libqt6serialport6-dev \
    libspatialindex-dev \
    libspatialite-dev \
    libsqlite3-dev \
    libsqlite3-mod-spatialite \
    libzip-dev \
    libzstd-dev \
    ninja-build \
    protobuf-compiler \
    python3-dev \
    qtkeychain-qt6-dev \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-positioning-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-base-private-dev \
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

# environment variables shall be located in .docker/docker-variables.env

RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libqt6core6

#!/usr/bin/env bash

set -e

##############
# Setup ccache
##############
export CCACHE_TEMPDIR=/tmp
# Github workflow cache max size is 2.0, but ccache data get compressed (roughly 1/5?)
ccache -M 2.0G

# Temporarily uncomment to debug ccache issues
# export CCACHE_LOGFILE=/tmp/cache.debug
ccache -z

mkdir -p /usr/src/qgis/build-pyside6
cd /usr/src/qgis/build-pyside6 || exit 1

CLANG_WARNINGS="-Wrange-loop-construct"

export CC=/usr/lib/ccache/clang
export CXX=/usr/lib/ccache/clang++

cmake -GNinja \
 -DPYTHON_LIBRARY=/usr/lib64/libpython3.10.so.1.0 \
 -DENABLE_TESTING=ON \
 -DWITH_PYSIDE=ON \
 -DBUILD_WITH_QT6=ON \
 -DWITH_EPT=OFF \
 -DWITH_QUICK=OFF \
 -DWITH_3D=OFF \
 -DWITH_ANALYSIS=OFF \
 -DWITH_DESKTOP=OFF \
 -DWITH_GUI=OFF \
 -DWITH_STAGED_PLUGINS=ON \
 -DWITH_GRASS=OFF \
 -DWITH_QGIS_PROCESS=OFF \
 -DWITH_QTWEBKIT=OFF \
 -DWITH_QT5SERIALPORT=OFF \
 -DSUPPRESS_QT_WARNINGS=ON \
 -DENABLE_MODELTEST=ON \
 -DENABLE_PGTEST=OFF \
 -DENABLE_SAGA_TESTS=OFF \
 -DENABLE_MSSQLTEST=OFF \
 -DWITH_QSPATIALITE=OFF \
 -DWITH_QWTPOLAR=OFF \
 -DWITH_APIDOC=OFF \
 -DWITH_ASTYLE=OFF \
 -DWITH_CUSTOM_WIDGETS=OFF \
 -DWITH_DESKTOP=OFF \
 -DWITH_BINDINGS=OFF \
 -DWITH_SERVER=OFF \
 -DWITH_ORACLE=OFF \
 -DDISABLE_DEPRECATED=ON \
 -DCXX_EXTRA_FLAGS="${CLANG_WARNINGS}" \
 -DCMAKE_C_COMPILER=/bin/clang \
 -DCMAKE_CXX_COMPILER=/bin/clang++ \
 -DADD_CLAZY_CHECKS=ON \
 -DWERROR=TRUE \
 ..

ninja qgis_core pyqgis_core pyutils pyqtcompat pytesting
ninja install

########################
# Show ccache statistics
########################
echo "ccache statistics"
ccache -s

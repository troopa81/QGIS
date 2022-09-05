#!/usr/bin/env bash

sudo apt install -y \
     clang-tidy \
     python3-colorama

/root/QGIS/.docker/docker-qgis-configure.sh

cd ${CTEST_SOURCE_DIR}

/root/QGIS/scripts/clang-tidy.py --build ${CTEST_BUILD_DIR} --src_dir ${CTEST_SOURCE_DIR} $@

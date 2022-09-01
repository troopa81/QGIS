#!/usr/bin/env bash

/root/QGIS/.docker/docker-qgis-configure.sh

/root/QGIS/scripts/clang-tidy.py --build ${CTEST_BUILD_DIR} --src_dir ${CTEST_SOURCE_DIR} $@

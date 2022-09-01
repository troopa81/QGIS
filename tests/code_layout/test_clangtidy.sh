#!/bin/bash

for i in $@; do echo test1: $i; done

files=$@
for i in ${files[@]}; do echo test1: $i; done

for file in ${files[@]}; do [[ $file =~ .*\.(cpp|h)$ ]] && filtered+=("$file"); done
for i in ${filtered[@]}; do echo test1: $i; done

/root/QGIS/.docker/docker-qgis-configure.sh

cd /root/QGIS/

clang-tidy -p=build -checks="bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-virtual-near-miss" ${filtered[@]}


# Even with line-filter there is no way some time to get rid of some non user code
# See issue https://bugs.llvm.org/show_bug.cgi?id=38484
# So we remove them manually

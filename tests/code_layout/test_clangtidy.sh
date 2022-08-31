#!/bin/bash

for i in $@; do echo test1: $i; done

exit 0
clang-tidy -p=build -checks="bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-virtual-near-miss" $@

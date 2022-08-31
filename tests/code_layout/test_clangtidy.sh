#!/bin/bash

ls build

clang-tidy -p=build -checks="bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-virtual-near-miss" ${filtered[@]}

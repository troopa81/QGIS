#!/usr/bin/env python3

#TODO add cartouche

import subprocess
import re
import sys
import argparse
import os

parser = argparse.ArgumentParser(description='Start clang-tidy on source files')
parser.add_argument('sources', metavar='source', type=str, nargs='+',
                    help='source files')
parser.add_argument('--build', action='store',
                    help='build directory containing the compile_commands.json file (default: current directory)')
parser.add_argument('--src_dir', action='store',
                    help='source directory (default: current directory)')

args = parser.parse_args()


sources = [source for source in args.sources if source.endswith(".cpp") or source.endswith(".h")]
build_dir = args.build if args.build else os.getcwd()
src_dir = args.src_dir if args.src_dir else os.getcwd()

print("Starting Clang-tidy")
print("- Sources     : ")
for source in sources:
    print("   * {}".format(source))
print("- Build dir   : {}".format(build_dir))
print("- Sources dir : {}".format(src_dir))

if not len(sources):
    print("No sources to check!")
    exit(0)

command = ["clang-tidy", "-p={}".format(build_dir),
           "-checks=bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-virtual-near-miss"] + sources

# Even with line-filter there is no way some time to get rid of some non user code
# See issue https://bugs.llvm.org/show_bug.cgi?id=38484

with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1,
                      universal_newlines=True) as p:
    for line in p.stdout:
        if re.search(": warning:", line):
            filtered = not line.startswith(src_dir)

        if filtered:
            continue

        print(line, end='')

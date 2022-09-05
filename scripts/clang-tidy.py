#!/usr/bin/env python3

#TODO add cartouche

import subprocess
import re
import sys
import argparse
import os
import colorama
from colorama import Fore, Style

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

base_command = ["clang-tidy", "-p={}".format(build_dir),
                "-checks=bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-virtual-near-miss"]

# Don't give all sources at once to clang-tidy, otherwise it will print errors only when everything is finished
error_types = {}
for source in sources:

    command = base_command + [source]
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1,
                          universal_newlines=True) as p:
        filtered = False
        for line in p.stdout:
            result = re.search("(.*): warning:(.*)\[(.*)\]$", line)
            if result:

                where, error_msg, error_type = result.groups()

                # Even with line-filter there is sometime no way to get rid of some non user code warnings
                # See issue https://bugs.llvm.org/show_bug.cgi?id=38484
                filtered = not line.startswith(src_dir)

                if not filtered:
                    error_types[error_type] = error_types.get(error_type, 0) + 1

            if filtered:
                continue

            if result:
                print(f"{Style.BRIGHT}{where}: {Fore.RED}warning{Style.RESET_ALL}:{error_msg} {Fore.YELLOW}[{error_type}]{Style.RESET_ALL}")
            else:
                print(line, end='')


if len(error_types):
    print("\n\nReported warnings:")
    for error_type, count in error_types.items():
        print(f" - {Fore.YELLOW}{error_type:<50}{Style.RESET_ALL}: {count}")
else:
    print("No errors")

exit(len(error_types))

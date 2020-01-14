#!/usr/bin/python3

import subprocess
import shutil
import os
import sys

nb_repeat = 10
test_bin = '/home/julien/work/QGIS/build_debug/output/bin/qgis_taskmanagertest'

with open(os.devnull, 'w') as devnull:
    result = subprocess.check_output([test_bin, "-functions"], stderr=devnull)
tests = result.decode('utf-8').strip().replace("()", "").split("\n")

os.environ["RUN_FLAKY_TESTS"] = "true"

shutil.rmtree('/tmp/test_task_manager', ignore_errors=True)
os.mkdir('/tmp/test_task_manager')

for test in tests:
    print(" Launch {} ".format(test), end='')
    sys.stdout.flush()

    for i in range(nb_repeat):
        retcode = None
        try:
            result = subprocess.run([test_bin, test], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        except subprocess.TimeoutExpired:
            retcode = None
        else:
            retcode = result.returncode

        print("t" if retcode is None else ("." if retcode == 0 else "!"), end='')
        sys.stdout.flush()

        if retcode is not None and retcode != 0:
            with open('/tmp/test_task_manager/{}_{}.log'.format(test, i), 'w') as file:
                file.write(result.stdout.decode('utf-8'))
    print("")

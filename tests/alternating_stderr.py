#!/usr/bin/env python2.7

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    y = 10.0 * sin(x / 4.0)
    if x % 2 == 0:
        sys.stdout.write("+" * int(abs(y)))
        sys.stdout.write("\n")
        sys.stdout.flush()
    else:
        sys.stderr.write("+" * int(abs(y)))
        sys.stderr.write("\n")
        sys.stderr.flush()
    sleep(1)

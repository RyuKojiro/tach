#!/usr/bin/env python2.7

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    if x % 2 == 0:
        sys.stdout.write("+")
        sys.stdout.flush()
        if x % 15 == 0:
            sys.stdout.write("\n")
    else:
        sys.stderr.write("+")
        sys.stderr.flush()
        if x % 15 == 0:
            sys.stderr.write("\n")
    sleep(.3)

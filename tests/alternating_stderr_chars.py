#!/usr/bin/env python3

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    if x % 2 == 0:
        fd = sys.stdout
    else:
        fd = sys.stderr

    fd.write("+")
    fd.flush()

    if x % 15 == 0:
        sys.stdout.write("\n")
        sys.stderr.write("\n")
    sleep(.3)

#!/usr/bin/env python2.7

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    y = 10.0 * sin(x / 4.0)
    if x % 2 == 0:
        fd = sys.stdout
    else:
        fd = sys.stderr
    fd.write("+" * int(abs(y)))
    fd.write("\n")
    fd.flush()
    sleep(1)

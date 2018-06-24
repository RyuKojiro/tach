#!/usr/bin/env python2.7

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    y = 10.0 * sin(x / 4.0)
    print "+" * int(abs(y))
    if x % 5 == 0:
        print "this is a very long line" * 20
    sys.stdout.flush()
    sleep(abs(sin(x / 4.0) / 4.0))

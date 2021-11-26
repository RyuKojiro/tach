#!/usr/bin/env python3

from time import sleep
from math import sin
import sys

x = 0
while True:
    x = x + 1
    y = 10.0 * sin(x / 4.0)
    for i in range(0, int(abs(y))):
        sys.stdout.write("+")
        sys.stdout.flush()
        sleep(.3)
    print("nl")

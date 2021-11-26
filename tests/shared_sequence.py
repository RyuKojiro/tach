#!/usr/bin/env python3

from time import sleep
import sys

seq = [3,2,1,1,2,3,4]
names = ["stdout", "stderr" ]
fds = [sys.stdout, sys.stderr]

for i in range(len(seq)):
    fd = i % 2
    print((names[fd] + "\n") * seq[i], file=fds[fd], end="")
    fds[fd].flush()
    sleep(.01)

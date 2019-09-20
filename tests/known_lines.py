#!/usr/bin/env python3.7

from time import sleep
import sys

print("This is stdout", file=sys.stdout, end="\n")
sleep(.1)
print("and so is this", file=sys.stdout, end="\n")
sleep(.1)
print("but not this!", file=sys.stderr, end="\n")
sleep(.1)
print("back to stdout!", file=sys.stdout, end="\n")
sleep(.1)
print("and now back to stderr...", file=sys.stderr, end="\n")
sleep(.1)
print("for two lines", file=sys.stderr, end="\n")
sleep(.1)

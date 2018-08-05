#!/bin/sh

if [ -t 1 ];
then
   	echo tty
else
   	echo pipe
fi

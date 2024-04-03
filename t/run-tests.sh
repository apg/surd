#!/bin/sh

for n in $*; do echo "RUNNING: $n"; ./$n ; echo; done

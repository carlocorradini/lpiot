#!/usr/bin/bash
TESTID=$1

python $TESTBED_CLIENT download $1
tar -xf 'job_'$TESTID'.tar.gz'
rm 'job_'$TESTID'.tar.gz'

#!/usr/bin/bash
python $TESTBED_CLIENT schedule experiment.json 2>&1 | tee last-test.txt

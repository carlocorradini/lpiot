#!/usr/bin/bash
python3 $TESTBED_CLIENT schedule experiment.json 2>&1 | tee last-test.txt

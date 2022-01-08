#!/usr/bin/env python2.7
from __future__ import division

import re
import sys
import os.path
import argparse
import numpy as np
import pandas as pd
import csv
from datetime import datetime


NUM_SENSORS = 5

def parse_file(log_file, testbed=False):
    # Print some basic information for the user
    print(f"Logfile: {log_file}")
    print(f"{'Cooja simulation' if not testbed else 'Testbed experiment'}")

    # Create CSV output files
    fpath = os.path.dirname(log_file)
    fname_common = os.path.splitext(os.path.basename(log_file))[0]
    fenergest_name = os.path.join(fpath, f"{fname_common}-energest.csv")
    fenergest = open(fenergest_name, 'w')
    fenergest_writer = csv.writer(fenergest, dialect='excel')
    fexp_name = os.path.join(fpath, f"{fname_common}-exp.csv")
    fexp = open(fexp_name, 'w')
    fexp_writer = csv.writer(fexp, dialect='excel')

    # Write CSV headers
    fenergest_writer.writerow(["time", "node", "cnt", "cpu", "lpm", "tx", "rx"])
    fexp_writer.writerow(["time", "node", "type", "event_source", "event_seqn", "sensor"])

    # Regular expressions to match log lines (the initial record pattern changes in testbed wrt Cooja)
    if testbed:

        # Regex for testbed experiments
        record_pattern = r"\[(?P<time>.{23})\] INFO:firefly\.(?P<self_id>\d+): \d+\.firefly < b.*"
        regex_node = re.compile(r"{}'Rime configured with address "
                                r"(?P<src1>\d+).(?P<src2>\d+)'".format(record_pattern))
        regex_dc = re.compile(r"{}'Energest: (?P<cnt>\d+) (?P<cpu>\d+) "
                              r"(?P<lpm>\d+) (?P<tx>\d+) (?P<rx>\d+)'".format(record_pattern))
    else:

        # Regex for COOJA
        record_pattern = r"(?P<time>\d+).*ID:(?P<self_id>\d+).*"
        regex_node = re.compile(r"{}Rime started with address "
                                r"(?P<src1>\d+).(?P<src2>\d+)".format(record_pattern))
        regex_dc = re.compile(r"{}Energest: (?P<cnt>\d+) (?P<cpu>\d+) "
                              r"(?P<lpm>\d+) (?P<tx>\d+) (?P<rx>\d+)".format(record_pattern))
    # regex_trigger = re.compile(r"TRIGGER \[(?P<event_source>\w\w:\w\w), (?P<event_seqn>\d+)\]")
    regex_event = re.compile(r"{}EVENT \[(?P<event_source>\w\w:\w\w), (?P<event_seqn>\d+)\].*".format(record_pattern))
    regex_collect = re.compile(r"{}COLLECT \[(?P<event_source>\w\w:\w\w), (?P<event_seqn>\d+)\] (?P<source>\w\w:\w\w).*".format(record_pattern))
    regex_command = re.compile(r"{}COMMAND \[(?P<event_source>\w\w:\w\w), (?P<event_seqn>\d+)\] (?P<dest>\w\w:\w\w).*".format(record_pattern))
    regex_act = re.compile(r"{}ACTUATION \[(?P<event_source>\w\w:\w\w), (?P<event_seqn>\d+)\] (?P<dest>\w\w:\w\w).*".format(record_pattern))

    # Node list
    nodes = []

    # Parse log file and add data to CSV files
    with open(log_file, 'r') as f:
        for line in f:
            # Node boot
            m = regex_node.match(line)
            if m:
                # Get dictionary with data
                d = m.groupdict()
                node_id = int(d["self_id"])
                # Save data in the nodes list
                if node_id not in nodes:
                    nodes.append(node_id)
                # Continue with the following line
                continue

            # Energest Duty Cycle
            m = regex_dc.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                fenergest_writer.writerow([ts, d['self_id'], d['cnt'], d['cpu'], d['lpm'], d['tx'], d['rx']])
                continue

            # Event
            m = regex_event.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                fexp_writer.writerow([ts, d['self_id'], 'EVENT', d['event_source'], d['event_seqn'], d['event_source']])
                continue

            # Collect
            m = regex_collect.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                fexp_writer.writerow([ts, d['self_id'], 'COLLECT', d['event_source'], d['event_seqn'], d['source']])
                continue

            # Command
            m = regex_command.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                fexp_writer.writerow([ts, d['self_id'], 'COMMAND', d['event_source'], d['event_seqn'], d['dest']])
                continue

            # Actuation
            m = regex_act.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                fexp_writer.writerow([ts, d['self_id'], 'ACTUATION', d['event_source'], d['event_seqn'], d['dest']])
                continue

    # Close files
    fenergest.close()
    fexp.close()

    # Compute node duty cycle
    compute_node_duty_cycle(fenergest_name)
    exp_analysis(fexp_name)


def compute_node_duty_cycle(fenergest_name):

    # Read CSV file with dataframe
    df = pd.read_csv(fenergest_name, sep=',')

    # Discard first two Energest report
    df = df[df.cnt >= 2].copy()

    # Create new df to store the results
    resdf = pd.DataFrame(columns=['node', 'dc'])

    # Iterate over nodes computing duty cyle
    # print("\n----- Node Duty Cycle -----")
    nodes = sorted(df.node.unique())
    dc_lst = []
    for node in nodes:
        rdf = df[df.node == node].copy()
        total_time = np.sum(rdf.cpu + rdf.lpm)
        total_radio = np.sum(rdf.tx + rdf.rx)
        dc = 100 * total_radio / total_time
        # print("NODE: {}  -- Duty Cycle: {:.3f}%".format(node, dc))
        dc_lst.append(dc)
        # Store the results in the DF
        idf = len(resdf.index)
        resdf.loc[idf] = [node, dc]

    print("\n----- Duty Cycle Stats -----\n")
    print("AVERAGE DUTY CYCLE: {:.3f}%\nSTANDARD DEVIATION: {:.3f}"
          "\nMINIMUM: {:.3f}%\nMAXIMUM: {:.3f}%".format(np.mean(dc_lst),
                                                      np.std(dc_lst), np.amin(dc_lst), np.amax(dc_lst)))

    # Save DC dataframe to a CSV file (just in case)
    fpath = os.path.dirname(fenergest_name)
    fname_common = os.path.splitext(os.path.basename(fenergest_name))[0]
    fname_common = fname_common.replace('-energest', '')
    fdc_name = os.path.join(fpath, "{}-dc.csv".format(fname_common))
    # print("Saving Duty Cycle CSV file in: {}".format(fdc_name))
    resdf.to_csv(fdc_name, sep=',', index=False,
                 float_format='%.3f', na_rep='nan')


def exp_analysis(fexp_name):

    # Read CSV file with dataframe
    df = pd.read_csv(fexp_name, sep=',').astype(str)

    print("\n----- Reliability Stats -----\n")

    # Count of events, data collection rounds and failed events (no data collected)
    event_count = df[df.type == 'EVENT'].drop_duplicates().shape[0]
    collect_count = df[df.type == 'COLLECT'][['event_source', 'event_seqn']].drop_duplicates().shape[0]
    print(f"# EVENTS AT CONTROLLER: {event_count}")
    print(f"# COLLECT ROUNDS AT CONTROLLER: {collect_count}")
    print(f"# FAILED EVENTS: {event_count - collect_count}\n")

    # Collection reliability
    collect_rx = df[df.type == 'COLLECT'].groupby(['event_source', 'event_seqn']).size()
    print(f"COLLECT PDR: {round(collect_rx.mean()/NUM_SENSORS, 4)}\n")

    # Count of commands generated by the sink and received by any actuator
    cmd_df_nodup = df[df.type == 'COMMAND'].drop_duplicates(subset=['event_source', 'event_seqn', 'sensor'])
    act_df_nodup = df[df.type == 'ACTUATION'].drop_duplicates(subset=['event_source', 'event_seqn', 'sensor'])
    command_count = cmd_df_nodup.shape[0]
    actuation_count = act_df_nodup.shape[0]
    print(f"# COMMANDS GENERATED BY THE CONTROLLER: {command_count}")
    print(f"# COMMANDS RECEIVED BY ACTUATORS: {actuation_count}")

    # Average actuation reliability
    print(f"AVERAGE ACTUATION PDR: {round(actuation_count/command_count, 4)}\n")

    # Per-node actuation reliability
    sensors = sorted(df.sensor.unique())
    for sensor in sensors:
        n_command = cmd_df_nodup[cmd_df_nodup.sensor == sensor].shape[0]
        n_actuation = act_df_nodup[act_df_nodup.sensor == sensor].shape[0]
        if n_command == 0:
            print(f"SENSOR {sensor} -- no commands generated.")
        else:
            print(f"SENSOR {sensor} -- ACTUATION PDR: {round(n_actuation/n_command, 4)}")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('logfile', action="store", type=str,
                        help="etc logfile to be parsed and analyzed.")
    parser.add_argument('-t', '--testbed', action='store_true',
                        help="flag for testbed experiments")
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    print(args)

    if not args.logfile:
        print("Log file needs to be specified as 1st positional argument.")
    if not os.path.exists(args.logfile):
        print("The logfile argument {} does not exist.".format(args.logfile))
        sys.exit(1)
    if not os.path.isfile(args.logfile):
        print("The logfile argument {} is not a file.".format(args.logfile))
        sys.exit(1)

    # Parse log file, create CSV files, and call other functions to compute some stats
    parse_file(args.logfile, testbed=args.testbed)

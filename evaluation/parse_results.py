#!/usr/bin/python

"""
parse results from intranode communication test, based on libvchan.
the results have the following format:

bytes=16777216 , gran=512 , ring=524288 , Reader_time=14259 , Writer_time=14751
bytes=16777216 , gran=1024 , ring=524288 , Reader_time=13298 , Writer_time=13896
bytes=16777216 , gran=2048 , ring=524288 , Reader_time=12402 , Writer_time=13020
bytes=16777216 , gran=4096 , ring=524288 , Reader_time=12053 , Writer_time=12697
...
#we create a dictionary as follows:
 stats_ring = { bytes: { gran: { ring: { "read": read_time, "write": write_time }}}}
for example:
 stats_ring = { '1048576' : { '512': { '524288': { "read": 5214, "write": 5420}}}}
"""

import os
import sys

try:
    filename = sys.argv[1]
except:
    print "error reading filename"
    exit(1)

f = open(sys.argv[1])
res = f.read()
lines = res.split('\n')
nbytes_list = []
gran_list = []
ring_list = []
stats_ring = {}
for l in lines:
    if not l:
        continue
    if 'init' in l:
        continue
    metrics = l.split(' , ')
    for m in metrics:
        value = m.split('=')
        if 'bytes' in value[0]:
            nbytes = value[1]
            if nbytes not in nbytes_list:
                nbytes_list.append(value[1])
        else: 
            if 'gran' in value[0]:
                gran = value[1]
                if gran not in gran_list:
                    gran_list.append(value[1])
            else:
                if 'ring' in value[0]:
                    ring = value[1]
                    if ring not in ring_list:
                        ring_list.append(ring)
                    else:
                        continue
                else: 
                    continue
    temp_struct = {}
    for m in metrics:
        value = m.split('=')
        temp_struct[value[0]] = value[1]
        
    for i in nbytes_list:
        nbytes = temp_struct['bytes']
        gran = temp_struct['gran']
        ring = temp_struct['ring']
        if stats_ring.has_key(nbytes):
            if stats_ring[nbytes].has_key(gran):
                stats_ring[nbytes][gran][ring] = { 'write': temp_struct['Writer_time'], 'read': temp_struct['Reader_time']}
            else:
                stats_ring[nbytes][gran] = {ring: { 'write': temp_struct['Writer_time'], 'read': temp_struct['Reader_time']}}
        else:
            stats_ring[nbytes] = {gran:{ ring: { 'write': temp_struct['Writer_time'], 'read': temp_struct['Reader_time']}}}
        

#parse cmdline options. First one is the number of bytes, the second one is granularity. The third one is the ring
#If we don't specify any option, the parser will loop through all results

try:
    nbytes_list = [sys.argv[2]]
except:
    pass

try:
    gran_list_prev = gran_list
    gran_list = [sys.argv[3]]
except:
    pass

try:
    ring_list = [sys.argv[4]]
except:
    pass

if gran_list[0] == '0':
    gran_loop = 1
    gran_list = gran_list_prev
else:
    gran_loop = 0
    
sys.stderr.write("nbytes = %s, gran = %s, ring = %s\n" %(nbytes_list, gran_list, ring_list))

for i in nbytes_list:
    for g in gran_list:
        if not stats_ring[i].has_key(g):
            continue
        for r in ring_list:
            if not stats_ring[i].has_key(r):
                continue
            #Annotate each line to stderr, so that we are able to identify which metric 
            #we print each time
            sys.stderr.write("nbytes = %s, gran = %s, ring = %s\n" %(i, g, r))
            read_time = int(stats_ring[i][g][r]['read'])
            write_time = int(stats_ring[i][g][r]['write'])
            read_bw = float(i) / read_time
            write_bw = float(i) / write_time
            if gran_loop == 0:
                metric = r
            else:
                metric = g
            if int(metric) >= 1024:
                if int(metric) >= 1048576:
                    s="%dM" %(int (int(metric) / 1048576))
                else:
                    s="%dK" %(int (int(metric) / 1024))
            else:
                s = metric
            print "%s %f %f" %(s, read_bw, write_bw)

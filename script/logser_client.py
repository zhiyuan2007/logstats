import re,sys
import time
import commands
import urllib 
import urllib2 
import json
import datetime
import google.protobuf
from collections import defaultdict
import socket
import struct
import traceback
import os
from statsmessage_pb2 import *
QPS_TIME = 5
class Log_analy():
     '''
     {"count":1,"rate":8,"domain":"rb07.sycdn.kuwo.cn","logtime":"2015-11-10 17:34:00","miss_flux":538,"miss_count":1,"flux":538,
     "status":[{"rate":8,"count":1,"flux":538,"status_id":"502"},{"rate":8,"count":1,"flux":538,"status_id":"200"}],
     "distribution":[{"rate":8,"count":1,"area":"9","flux":538,"isp":"4"}]}
     '''
     def __init__(self, url, host):
         self.post_url = url
         self.host = host

     def read_buf_from_file(self, filename):
         fp = open(filename, "rb")
         reply_list = []
         while True:
             reply = StatsReply()
             len = fp.read(4)
             if len == '':
                break
             l, = struct.unpack("i", len)
             content = fp.read(l)
             reply.ParseFromString(content)
             reply_list.append(self.to_py_json(reply))

         return reply_list

     def get_all_stats_from_file(self, filename):
         reply = self.read_buf_from_file(filename)
         return reply

     def to_py_json(self, reply):
         all_stats = {}
         timestr = reply.timestr
         for i in range(len(reply.name)):
             type, name = reply.name[i].split(":", 1)
             if type == "view":
                 region, name = name.split(":")
             elif type == "code":
                 status_code, name = name.split(":")
                 
             if not all_stats.get(name, None):
                 all_stats[name] = {}
             if not all_stats[name].get("distribution", None):
                 all_stats[name]["distribution"] = []
             if not all_stats[name].get("status", None):
                 all_stats[name]["status"] = []
             if type == "name":
                 all_stats[name]['domain'] = name
                 all_stats[name]['logtime'] = timestr 
                 all_stats[name]['rate'] = reply.rate[i] 
                 all_stats[name]['flux'] = reply.bandwidth[i] 
                 all_stats[name]['count'] = reply.access_count[i] 
                 all_stats[name]['hit_count'] = reply.hit_count[i] 
                 all_stats[name]['hit_flux'] = reply.hit_bandwidth[i] 
                 all_stats[name]['miss_count'] = reply.lost_count[i] 
                 all_stats[name]['miss_flux'] = reply.lost_bandwidth[i] 
             elif type == "view":
                 temp_obj = {}
                 area, isp = region.split("\t")
                 temp_obj['area'] = area 
                 temp_obj['isp'] = isp 
                 temp_obj['rate'] = reply.rate[i] 
                 temp_obj['flux'] = reply.bandwidth[i] 
                 temp_obj['count'] = reply.access_count[i] 
                 temp_obj['hit_count'] = reply.hit_count[i] 
                 temp_obj['hit_flux'] = reply.hit_bandwidth[i] 
                 all_stats[name]["distribution"].append(temp_obj)
             elif type == "code":
                 temp_obj = {}
                 temp_obj['status_id'] = status_code
                 temp_obj['rate'] = reply.rate[i] 
                 temp_obj['flux'] = reply.bandwidth[i] 
                 temp_obj['count'] = reply.access_count[i] 
                 all_stats[name]["status"].append(temp_obj)

         return all_stats.values()

     def post_data_to_server(self, stats_data):   
         post_data = "host=" + self.host + "&data=" + json.dumps(stats_data)
         print "send data", post_data 
         try:
             req = urllib2.Request(self.post_url, post_data)
             response = urllib2.urlopen(req)
             return response.read()
         except Exception, e:
             print "post data failed", e
             return "failed"
     
     def send_data_from_file(self, filename):
         stats_data_list = self.get_all_stats_from_file(filename) 
         result = ''
         for sd in stats_data_list:
            result = self.post_data_to_server(sd)
         return result

def main():
    if len(sys.argv) != 4:
        print "usage: logser_client.py post_url data_dir backup_dir"
        return
    post_url = sys.argv[1] 
    stats_dir = sys.argv[2]
    backup_dir = sys.argv[3]
    host= socket.gethostname()
    log = Log_analy(post_url, host)
    os.chdir(stats_dir)
    while True:
        files = os.listdir(stats_dir)
        files.sort()
        for filename in files:
           ret = log.send_data_from_file(filename)
           print "send file %s, ret %s" % (filename, ret)
           if ret.find("success"): 
               os.rename(filename, os.path.join(backup_dir, filename))

        time.sleep(63);

if __name__ == "__main__":
     main()

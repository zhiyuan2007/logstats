import os
import struct
import json
import re,sys
import google.protobuf
from statsmessage_pb2 import *
class StatsReader():
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
        reply_list = self.read_buf_from_file(filename)
        return reply_list
        
    def merge_distribution(self, d1_list, d2_list):
        pass
    def merge_status(self, s1_list, s2_list):
        #for st1 in s1_list:
        #   for st2 in s2_list:
        pass
    def merge_stats(self, rep1, rep2):
        print rep1
        rep = {}
        rep["count"] = rep1["count"] + rep2["count"]
        rep["domain"] = rep1["domain"]
        rep["logtime"] = rep1["logtime"]
        rep["hit_count"] = rep1["hit_count"] + rep2["hit_count"]
        rep["hit_flux"] = rep1["hit_flux"] + rep2["hit_flux"]
        rep["miss_count"] = rep1["miss_count"] + rep2["miss_count"]
        rep["miss_flux"] = rep1["miss_flux"] + rep2["miss_flux"]
        rep["flux"] = rep1["flux"] + rep2["flux"] 
        rep["rate"] = rep1["rate"] + rep2["rate"]
        rep["stauts"] = slef.merge_status(rep1["status"], rep2["status"])
        rep["distribution"] = slef.merge_distribution(rep1["distribution"], rep2["distribution"])
        return rep

    def reduce_map(self, reply_list):
        print reply_list
        reduce(self.merge_stats, reply_list)
    
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
                area_id, isp_id = re.split("\s", region)
                temp_obj['isp'] = isp_id 
                temp_obj['area'] = area_id
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

def main(): 
    if len(sys.argv) != 2:
        print "usage %s filename" % (sys.argv[0])
        return
    filename = sys.argv[1]
    log = StatsReader()
    stats_data = log.get_all_stats_from_file(filename) 
    print "data package %d count" % (len(stats_data))
    for sd in stats_data:
        print "data:", sd #json.dumps(sd, indent=False)

if __name__ == "__main__":
     main()

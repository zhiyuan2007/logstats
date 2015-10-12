import datetime
import threading
import socket
import urllib2
import db_manager
import time, json
lock = threading.Lock()
response_list = []
class StatsHost():
     def __init__(self, name):
         self.name = name
         self.init_stas()

     def init_stas(self):
         self.stats = {}
         self.stats["bw_num"] = []
         self.stats["bw_ratio"] = []
         self.stats["transmit_flow"] = 0
         self.stats["app_flow"] = 0
         self.stats["ratio_of_app_with_tx"] = 0

     def handle_response(self, response):
         if len(response["bandwidth"]) == 0:
            self.init_stas()
            return

         total_flow = response["bandwidth"][0][1]
         ratio_list = map(lambda x: [x[0], "%0.3f" % (x[1] * 1.0 / total_flow)], response["bandwidth"])
         self.stats["bw_num"] = response["bandwidth"]
         self.stats["bw_ratio"] = ratio_list
         self.stats["transmit_flow"] = response["network_bandwidth"] 
         self.stats["app_flow"] = total_flow * 1000
         self.stats["ratio_of_app_with_tx"] = "%0.3f" % (self.stats["app_flow"]  * 1.0/ self.stats["transmit_flow"])

     def get_stats(self):
         return self.stats 
     
     def get_one_of_stats(self, key):
         if key in self.stats.keys():
             return self.stats[key]

         return "invalid key %s " % (key)

class StatsManager():
     def __init__(self, nodes, dbfile, time_interval):
         self.nodes = nodes
         self.dm = db_manager.DB(dbfile)
         self.total = []
         self.time_interval = time_interval
         self.timer = None
         self.host_info = {}
         map(self._init_host_info, self.nodes)

     def _init_host_info(self, node):
         hstats = StatsHost(node)
         self.host_info[node] = hstats

     def start_timer(self, paras):
         self.timer = threading.Timer(self.time_interval, self.get_bandwidth_of_all_node, paras)
         self.timer.start()

     def total_bandwidth(self):
         return self.total

     def get_one_stats_by_host(self, hostname, key):
         return self.host_info[hostname].get_one_of_stats(key)

     def get_stats_by_host(self, hostname):
         return self.host_info[hostname].get_stats()

     def free_resource(self, signal, frame):
         self.timer.cancel()

     def get_bandwidth_of_all_node(self, *args):
         global response_list
         response_list = []
         threads = []
         ts = int(time.time())
         for node in self.nodes:
            t = threading.Thread(target=self.get_bandwidth_of_node, args=(node,ts))
            t.setDaemon(True)
            threads.append(t)
            t.start()
     
         for t in threads:
            t.join(30)

         save_or_not = args[0]
         if save_or_not:
             self._insert_to_db(response_list, ts)
         self._calcu_summary(response_list)
         self.timer = threading.Timer(self.time_interval, self.get_bandwidth_of_all_node, args)
         self.timer.start()
  
     def get_bandwidth_of_node(self, node, ts):
         url = "http://%s:8000/views/all/stats/bandwidth_all" % (node)
         response = urllib2.urlopen(url, timeout=40)
         html = response.read()
         bandwidth = json.loads(html)
         lock.acquire()
         print "host %s, bandwidth %s" % (node, bandwidth)
         bandwidth["host"] = node
         self.host_info[node].handle_response(bandwidth)
         response_list.append(bandwidth)
         lock.release()
     
         return bandwidth
         
     def _merge_item(self, x, y):
         return dict([[k , x.get(k, 0) + y.get(k, 0)] for k in set(x) | set(y)])
     
     def _insert_to_db(self, response_list, ts):  
         for response in response_list:
             self.dm.insert_bandwidth_batch(response["host"],  response["bandwidth"], ts) 
             self.dm.insert_flow(response["host"], response["network_bandwidth"], ts)
         self.dm.commit()

     def _calcu_summary(self, response_list):
         _bandwith_list = map(lambda x: dict(x["bandwidth"]), response_list)
         total_bandwidth = reduce(self._merge_item, _bandwith_list)
         sorted_bw = sorted(total_bandwidth.iteritems(), key=lambda(k, v) : v, reverse=True)
         self.total = sorted_bw


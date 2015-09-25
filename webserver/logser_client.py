import re,sys
import time
import commands
import threading
import datetime
import google.protobuf
from collections import defaultdict
import socket
import struct
import traceback
from statsmessage_pb2 import *
QPS_TIME = 5
class Log_analy():
     domaintopn = 'domaintopn'
     iptopn = 'iptopn'
     viewqps = 'qps'
     totalquery = "totalquery"
     rtype = 'rtype'
     rcode = 'rcode'
     def __init__(self, ip="127.0.0.1", port=8999):
         self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         self.ip = ip
         self.port = port
         self.sock.connect((ip, port))
         print "connect %s %d success" % (ip, port)
         self.connecting = True
         self.mutex = threading.Lock()
         self.retry_count = 0

     def _read_buf(self, size):
         n = size
         buf = ''
         while n > 0:
             data = self.sock.recv(n)
             if data == '':
                 raise RuntimeError('unexpected connection close')
             buf += data
             n -= len(data)
         return buf

     def _reconncet(self):
         self.retry_count = 0
         while (not self.connecting and self.retry_count < 3):
             try:
                 self.sock.close()
                 self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                 self.sock.connect((self.ip, self.port))
                 self.connecting = True
             except Exception, data:
                 if str(data).find("Transport endpoint is already connected") != -1:
                     self.connecting = True
                 else:
                     print traceback.print_exc()
                     print "retry time is %d, ip %s port %d" % (self.retry_count, self.ip, self.port)
                     self.retry_count += 1
                     time.sleep(1)

     def _send_request(self, request):
         self.mutex.acquire()
         if not self.connecting: self._reconncet()
         reply = StatsReply()
         try:
             size   = request.ByteSize()
             print "send key: %s view: %s length %d" % (request.key, request.view, size)
             size_t = struct.pack('!I', size)
             self.sock.send(size_t)
             s = request.SerializeToString()
             self.sock.send(s)
             
             self.sock.settimeout(3.0)
             r = self.sock.recv(4)
             size = struct.unpack('!I', r)
             print "receive length %d" % size[0]
             buf = self._read_buf(size[0])
             reply.ParseFromString(buf)
         except socket.timeout:
             reply.key = request.key
             print "waiting response timeout reconnect"
             sys.stdout.flush()
             self.connecting = False
         except socket.error:
             reply.key = request.key
             self.connecting = False
         except Exception, e:
             reply.key = request.key
             print "exception: ", e
             sys.stdout.flush()
             self.connecting = False
         finally:
             self.mutex.release()
         return reply

     def execute_topn_query(self, view, key, topn):
         req = StatsRequest()
         req.key = key 
         req.view = view
         req.topn = int(topn)
         reply = self._send_request(req)
         if key.lower() == "iptopn":
             return [[reply.name[i][0:-1], reply.count[i]] for i in range(0, len(reply.name))]
         return [[reply.name[i], reply.count[i]] for i in range(0, len(reply.name))]
             

     def get_property(self, view, key):
         req = StatsRequest()
         req.key = key 
         req.view = view
         reply = self._send_request(req)
         r = [[reply.name[i], reply.count[i]] for i in range(0, len(reply.name))]
         return sorted(r, key=lambda(k,v):v, reverse=True)

     def get_bandwidth_all(self):
         req = StatsRequest()
         req.key = "bandwidth_all"
         req.view = "*" 
         reply = self._send_request(req)
         r = [[reply.name[i], reply.count[i]] for i in range(0, len(reply.name))]
         return sorted(r, key=lambda(k,v):v, reverse=True)

     def get_view_qps(self, view):
         return self._get_float(view, "qps")

     def get_success_rate(self, view):
         return self._get_float(view, "success_rate")

     def get_bandwidth(self, view):
         return self._get_float(view, "bandwidth")

     def _get_float(self, view, key):
         req = StatsRequest()
         req.key = key 
         req.view = view
         reply = self._send_request(req)
         
         if reply.maybe == "view not find":
             if key == "qps":
                 return {key: 0.0}
             elif key == "success_rate":
                 return {key: 1.0}
             else:
                 return {key : "not found view"}
         try:
             if (key == 'success_rate' and (float(reply.maybe) < 0.1)):
                 return {key: 1.0}
             else:
                 return {key:float(reply.maybe)}
         except:
             print "convert exception: key %s reply %s" % (key, reply.maybe)
             if key == "qps":
                 return {key: 0.0}
             else:
                 return {key: 1.0}
        
     def get_latest_log(self, key="latest_log", num=1000):
         result = commands.getoutput("cat /usr/local/zddi/dns/log/query.log.|grep -v \"127.0.0.1\" |tail -n 1000")
         latest_log = result.split("\n")
         final  = []
         i = 0
         for log in latest_log:
             elem = {}
             elem['log'] = log
             elem['id'] = i
             final.append(elem)
             i += 1
         return final
     def get_query_speed_log(self):
         req = StatsRequest()
         req.key = "speedlog"
         req.view = '*'
         reply = self._send_request(req)
         return [r for r in reply.name]
     
     def get_views(self):
         req = StatsRequest()
         req.key = "views"
         req.view = 'all'
         reply = self._send_request(req)
         return [r for r in reply.name]

     def flush_data(self):
         req = StatsRequest()
         req.key = "flush"
         req.view = 'all'
         reply = self._send_request(req)
         return [r for r in reply.name]

     def reload_conf(self):
         req = StatsRequest()
         req.key = "reload"
         req.view = 'all'
         reply = self._send_request(req)
         return [r for r in reply.name]

if __name__ == "__main__":
    log = Log_analy("127.0.0.1", 8999)
    #print log.get_latest_log()
    #print "topn ", log.execute_topn_query("default", "domaintopn", 1000)
    #import time
    #time.sleep(1)
    print "views", log.get_views()
    print "success ", log.execute_topn_query("*", "iptopn", 100)
    print "success ", log.execute_topn_query("default", "iptopn", 100)
    print "success ", log.execute_topn_query("default", "domaintopn", 100)
    print "success ", log.execute_topn_query("*", "domaintopn", 100)
    print "success ", log.get_property("*", "rtype")
    print "success ", log.get_property("default", "rcode")
    print "success ", log.get_view_qps("default")
    print "success ", log.get_success_rate("*")
    print "success ", log.flush_data()
    print "error ", log.get_view_qps("notview1")
    print "error ", log.execute_topn_query("notviewip", "iptopn", 100)
    print "error ", log.execute_topn_query("notview2", "domaintopn", 100)
    print "error ", log.get_property("notview3", "rtype")
    print "error ", log.get_property("notview4", "rcode")
    print "error ", log.get_property("notview4", "success_rate")
    print "success", log.get_query_speed_log()

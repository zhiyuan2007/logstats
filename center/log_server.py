import datetime
import os, sys, time, json
import threading
import socket
import urllib2
import tornado
import tornado.web
import tornado.ioloop
import tornado.options
import tornado.httpserver
from tornado.options import define, options

define("slaveport", default=8001, help="run on the given port", type=int)
define("uri", default='views/all/stats/bandwidth_all', help="url")
class BandwidthAllHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, key):
        nodes = self.application.settings.get('nodes')
        if key == "bandwidth_all":
            self.write(json.dumps(get_bandwidth_of_all_node(nodes)))
        elif key == "qps_all":
            self.write(json.dumps(get_qps_of_all_node(nodes)))

class OperationHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, operation):
        if operation == "flush":
            self.write("ok")
        elif operation == "views":
            self.write("ok")
        elif operation == "reload":
            self.write("ok")

def get_host_from_file(filename):
   fp = open(filename, "r")
   nodes = []
   for line in fp.readlines():
       if line.startswith("#"):continue
       info = line.strip()
       nodes.append(info)
   fp.close()
   return nodes
def get_bandwidth_of_node(node):
    url = "http://%s:8000/views/all/stats/bandwidth_all" % (node)
    response = urllib2.urlopen(url, timeout=10)
    html = response.read()
    bandwidth = json.loads(html)
    return bandwidth
    
def merge_item(x, y):
    return dict([[k , x.get(k, 0) + y.get(k, 0)] for k in set(x) | set(y)])

def get_bandwidth_of_all_node(nodes):
    bandwidth_list = []
    for node in nodes:
       bw_dict = get_bandwidth_of_node(node) 
       bandwidth_list.append(dict(bw_dict))
    
    total_bandwidth = reduce(merge_item, bandwidth_list)

    
    sorted_bw = sorted(total_bandwidth.iteritems(), key=lambda(k, v) : v, reverse=True)

    return sorted_bw 

def main():
    try:
        tornado.options.parse_command_line()
        nodes = get_host_from_file("nodes.conf")
        application = tornado.web.Application([
            ('/views/all/stats/(bandwidth_all|qps_all)', BandwidthAllHandler),
            ('/views/all/stats/(views|reload|flush)', OperationHandler),
            ], **{'nodes': nodes})
        http_server = tornado.httpserver.HTTPServer(application)
        http_server.listen(options.slaveport, address="0.0.0.0")
        tornado.ioloop.IOLoop.instance().start()
        print "server start at port: ", options.port
        sys.stdout.flush()
    except KeyboardInterrupt:
        print 'stop by keyboard'
    except Exception, data:
        print Exception, ":", data
        sys.stdout.flush()

if __name__ == "__main__":
    main()

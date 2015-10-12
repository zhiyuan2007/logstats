import signal
import os, sys, time, json
import traceback
import tornado
import tornado.web
import tornado.ioloop
import tornado.options
import tornado.httpserver
from tornado.options import define, options
from stats_manager import StatsManager
define("port", default=8001, help="run on the given port", type=int)
define("hostfile", default="nodes.conf", help="hostname list file", type=str)
define("dbfile", default="stats.db", help="db file", type=str)
define("interval", default=60, help="interval", type=int)
define("save", default=False, help="save it to db or not", type=bool)
class StatsAllHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, key):
        sm = self.application.settings.get('statsmanager')
        if key == "bandwidth_all":
            self.write(json.dumps(sm.total_bandwidth()))
        elif key == "qps_all":
            self.write(json.dumps(sm.get_qps_of_all_node()))

class StatsEachHostHandler(tornado.web.RequestHandler):
    def get(self, host, key):
        sm = self.application.settings.get('statsmanager')
        if key == "qps_all":
            self.write(json.dumps(sm.get_qps_of_all_node()))
        elif key == "all":
            self.write(json.dumps(sm.get_stats_by_host(host)))
        else:
            self.write(json.dumps(sm.get_one_stats_by_host(host, key)))


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

def main():
    try:
        tornado.options.parse_command_line()
        nodefile =  options.hostfile
        dbfile  = options.dbfile
        time_interval = options.interval
        nodes = get_host_from_file(nodefile)
        sm = StatsManager(nodes, dbfile, time_interval)
        sm.start_timer((options.save,))
        application = tornado.web.Application([
            ('/views/all/stats/(bandwidth_all|qps_all)', StatsAllHandler),
            ('/hosts/(.*)/stats/(all|bw_ratio|bw_num|transmit_flow|ratio_of_app_with_tx|qps_all)', StatsEachHostHandler),
            ('/views/all/stats/(views|reload|flush)', OperationHandler),
            ], **{'statsmanager': sm})
        http_server = tornado.httpserver.HTTPServer(application)
        http_server.listen(options.port, address="0.0.0.0")
        tornado.ioloop.IOLoop.instance().start()
        print "server start at port: ", options.port
        sys.stdout.flush()
        print os.getpid()
    except KeyboardInterrupt:
        print 'stop by keyboard'
        os.kill(os.getpid(), signal.SIGKILL)
    except Exception, data:
        print Exception, ":", data
        print traceback.print_exc()
        os.kill(os.getpid(), signal.SIGKILL)
        sys.stdout.flush()

if __name__ == "__main__":
    main()

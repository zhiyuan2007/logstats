import datetime
import os, sys, time, json
import authority_template
import db_handler
import logser_client 
import threading
import socket
import tornado
import tornado.web
import tornado.ioloop
import tornado.options
import tornado.httpserver
from log_tools import *
from tornado.options import define, options

define("port", default=8000, help="run on the given port", type=int)
define("redisport", default=6379, help="redis run on the given port", type=int)
define("redishost", default='localhost', help="run on this server")
define("redisunixsocket", default='/usr/local/redis.sock', help="connect redis use unix socket") #/tmp/redis.sock /tmp/redis_6380.sock
filename="syslog_log.conf"
db_name = 'topn_and_qps.db'
FLUSH_TIME = 300
HALF_HOUR_TIME = 1800
ONE_HOUR_TIME = 3600
RECORD_NUM = 1000
ONE_DAY_TIME = 86400
class StatsThread(threading.Thread):
    def __init__(self, db, logserver_client):
        threading.Thread.__init__(self)
        self.keep_running = 1 
        self.db = db
        self.logserver_client = logserver_client
        self.times = 0
        self.first = True

    def run(self):
        while self.keep_running:
          try:
            views = self.logserver_client.get_views()
            today = datetime.datetime.now()
            timestamp = today.strftime('%Y%m%d%H%M%S')
            timestamp = self.whole_point(timestamp)
            print "save data to db %d times date %s" % (self.times, timestamp)
            sys.stdout.flush()

            self.save_five_minute_data(views, timestamp)

            if not self.first:
                print "stats start %d times date %s" % (self.times, timestamp)
                if self.times % (HALF_HOUR_TIME/FLUSH_TIME) == 0:
                    self.save_half_hour_data(views, timestamp, today)

                if self.times % (ONE_HOUR_TIME/FLUSH_TIME) == 0:
                    self.save_one_hour_data(views, timestamp)
                    self.logserver_client.flush_data()

                if self.times % (ONE_DAY_TIME/FLUSH_TIME) == 0:
                    self.save_one_day_data(views, timestamp, today)
                    three_month_day = datetime.timedelta(days=-90)
                    some_time_ago = (today + three_month_day).strftime('%Y%m%d%H%M%S')
                    self.db.delete_data_long_ago(some_time_ago)
                    self.db.delete_data_longlong_ago(some_time_ago)

                self.db.commit()
          except Exception, data:
              print Exception, ":", data
              import traceback
              print traceback.print_exc()
              sys.stdout.flush()
          finally:
              if self.first:
                  self.first = False
              self.times += 1
              time.sleep(FLUSH_TIME)

    def stopme(self):
        self.keep_running = 0 

    def whole_point(self, time1):
        minute = int(time1[-4:-2])
        whole = minute - minute % 5
        return time1[0:10] + "%02d" % whole + '00'

    def save_five_minute_data(self, views, timestamp):
        for view in views:
            qps = self.logserver_client.get_view_qps(view)['qps']
            self.db.insert_qps(view, timestamp, qps)
            success_rate = self.logserver_client.get_success_rate(view)['success_rate']
            self.db.insert_success_rate(view, timestamp, success_rate)

    def save_one_hour_data(self, views, timestamp):
        #--------------------one hour-----------------#
        for view in views:
            domaintopn = self.logserver_client.execute_topn_query(view, "domaintopn", RECORD_NUM)
            iptopn = self.logserver_client.execute_topn_query(view, "iptopn", RECORD_NUM)
            rcode = self.logserver_client.get_property(view, "rcode")
            rtype = self.logserver_client.get_property(view, "rtype")
            self.db.insert_domain_topn(view, timestamp, domaintopn)
            self.db.insert_ip_topn(view, timestamp, iptopn)
            self.db.insert_rcode(view, timestamp, rcode)
            self.db.insert_rtype(view, timestamp, rtype)

    def save_half_hour_data(self, views, timestamp, today):
        #--------------------half hour-----------------#
        _half_hour = datetime.timedelta(seconds=-1800)
        start_date = (today + _half_hour).strftime('%Y%m%d%H%M%S')
        start_date = self.whole_point(start_date)
        end_date = timestamp
        for view in views:
            records = self.db.get_qps(view, start_date, end_date)
            rec_len = len(records)
            if rec_len != 0:
                sum = 0.0
                for i in range(0, rec_len):
                    sum += records[i][1]
                self.db.insert_one_day_qps(view, end_date, sum/rec_len*1.0)
            else:
                self.db.insert_one_day_qps(view, end_date, 0.0)
            records = self.db.get_success_rate(view, start_date, end_date)
            rec_len = len(records)
            if rec_len != 0:
                sum = 0.0
                for i in range(0, rec_len):
                    sum += records[i][1]
                self.db.insert_one_day_success_rate(view, end_date, sum/rec_len*1.0)
            else:
                self.db.insert_one_day_success_rate(view, end_date, 1.0)

    def save_one_day_data(self, views, timestamp, today):
        _one_day = datetime.timedelta(days=-1)
        start_date = (today + _one_day).strftime('%Y%m%d%H%M%S')
        start_date = self.whole_point(start_date)
        end_date = timestamp
        for view in views:
            #if view == "*" : continue
            iptopn = self.db.get_ip_topn(view,  start_date, end_date, RECORD_NUM)
            self.db.insert_one_day_ip_topn(view, timestamp, iptopn)
            dotopn = self.db.get_domain_topn(view,  start_date, end_date, RECORD_NUM)
            self.db.insert_one_day_domain_topn(view, timestamp, dotopn)
            rtype = self.db.get_rtype(view,  start_date, end_date)
            self.db.insert_one_day_rtype(view, timestamp, rtype)
            rcode = self.db.get_rcode(view,  start_date, end_date)
            self.db.insert_one_day_rcode(view, timestamp, rcode)
 
class SetHandler(tornado.web.RequestHandler):
    def post(self):
        para_dict = get_body_para(self.request.body)
        print para_dict
        sys.stdout.flush()
        path = self.application.settings.get('path')
        if path:
            return "dup"
        elif not para_dict.get("db_path"):
            self.send_error("need parameter db_path")
        else:
            path = para_dict.get("db_path")
            self.application.settings['path'] = path
            os.system("mkdir -p %s" % path)
        db_hand = db_handler.DB(path + '/' + db_name)
        self.application.settings['db_hand'] = db_hand
        logserver_client = self.application.settings.get('logserver_client')
        qps_thread = StatsThread(db_hand, logserver_client)
        qps_thread.daemon = True
        qps_thread.start()
    def put(self, *args, **kwargs):
        pass

class SuccessRateHandler(tornado.web.RequestHandler):
    def get(self, view):
        logserver_client = self.application.settings.get('logserver_client')
        db_hand = self.application.settings.get('db_hand')
        start_date = self.get_argument("start_date", None)
        end_date = self.get_argument("end_date", None)
        if start_date and end_date:
            self.write(json.dumps(db_hand.get_success_rate(view, start_date, end_date)))
        else:
            self.write(json.dumps(logserver_client.get_success_rate(view)))

class ViewqpsHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, view):
        logserver_client = self.application.settings.get('logserver_client')
        db_hand = self.application.settings.get('db_hand')
        start_date = self.get_argument("start_date", None)
        end_date = self.get_argument("end_date", None)
        if start_date and end_date:
            self.write(json.dumps(db_hand.get_qps(view, start_date, end_date)))
        else:
            self.write(json.dumps(logserver_client.get_view_qps(view)))

class BandwidthHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, view):
        logserver_client = self.application.settings.get('logserver_client')
        db_hand = self.application.settings.get('db_hand')
        start_date = self.get_argument("start_date", None)
        end_date = self.get_argument("end_date", None)
        if start_date and end_date:
            self.write(json.dumps(db_hand.get_qps(view, start_date, end_date)))
        else:
            self.write(json.dumps(logserver_client.get_bandwidth(view)))

class BandwidthAllHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self):
        logserver_client = self.application.settings.get('logserver_client')
        self.write(json.dumps(logserver_client.get_bandwidth_all()))

class OperationHandler(tornado.web.RequestHandler):
    def set_default_headers(self): 
        self.set_header('Access-Control-Allow-Origin', '*')
    def get(self, operation):
        logserver_client = self.application.settings.get('logserver_client')
        if operation == "flush":
            self.write(json.dumps(logserver_client.flush_data()))
        elif operation == "views":
            self.write(json.dumps(logserver_client.get_views()))
        elif operation == "reload":
            'ok'


class ViewHandler(tornado.web.RequestHandler):
    def get(self, view, key):
        topn = self.get_argument("topn", RECORD_NUM)
        logserver_client = self.application.settings.get('logserver_client')
        db_hand = self.application.settings.get('db_hand')
        start_date = self.get_argument("start_date", None)
        end_date = self.get_argument("end_date", None)
        if start_date and end_date:
            if key == "domaintopn":
                self.write(json.dumps(db_hand.get_domain_topn( view,  start_date, end_date, topn)))
            elif key == "iptopn":
                self.write(json.dumps(db_hand.get_ip_topn( view,  start_date, end_date, topn)))
        else:
            self.write(json.dumps(logserver_client.execute_topn_query(view, key, topn)))

class PropertyHandler(tornado.web.RequestHandler):
    def get(self, view, key):
        logserver_client = self.application.settings.get('logserver_client')
        db_hand = self.application.settings.get('db_hand')
        start_date = self.get_argument("start_date", None)
        end_date = self.get_argument("end_date", None)
        if start_date and end_date:
            if key == "rtype":
                self.write(json.dumps(db_hand.get_rtype(view,  start_date, end_date)))
            elif key == "rcode":
                self.write(json.dumps(db_hand.get_rcode(view,  start_date, end_date)))
        else:
            self.write(json.dumps(logserver_client.get_property(view, key)))

class LOGHandler(tornado.web.RequestHandler):
    def get(self, key):
        logserver_client = self.application.settings.get('logserver_client')
        if key == "query_logs":
            self.write(json.dumps(logserver_client.get_latest_log()))
        elif key == "query_speed_logs":
            self.write(json.dumps(logserver_client.get_query_speed_log()))

def main():
    try:
        tornado.options.parse_command_line()
        logserver_client = logser_client.Log_analy("127.0.0.1", 8999)
        application = tornado.web.Application([
            ('/service', SetHandler),
            ('/(query_logs|query_speed_logs)', LOGHandler),
            ('/views/(.*)/stats/success_rate', SuccessRateHandler),
            ('/views/(.*)/stats/qps', ViewqpsHandler),
            ('/views/(.*)/stats/bandwidth', BandwidthHandler),
            ('/views/all/stats/bandwidth_all', BandwidthAllHandler),
            ('/views/all/stats/(views|reload|flush)', OperationHandler),
            ('/views/(.*)/stats/(domaintopn|iptopn)', ViewHandler),
            ('/views/(.*)/stats/(rtype|rcode)', PropertyHandler),
            ], **{'logserver_client': logserver_client, 'db_hand':None })
        http_server = tornado.httpserver.HTTPServer(application)
        http_server.listen(options.port, address="::")
        if os.popen("ifconfig").read().split("lo")[0].count("inet addr") > 0:
            http_server.listen(options.port, address="0.0.0.0")
        tornado.ioloop.IOLoop.instance().start()
        print "server start at port: ", options.port
        sys.stdout.flush()
    except KeyboardInterrupt:
        print 'stop by keyboard'
    except Exception, data:
        print Exception, ":", data
        sys.stdout.flush()

if __name__ == "__main__":
    os.chdir("/tmp")
    main()

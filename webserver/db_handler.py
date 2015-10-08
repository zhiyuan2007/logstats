import sys
import time
import sqlite3
import threading
from collections import defaultdict
TIME_INTERVAL = 86400*5
MAX_POINTS = 1500

def synchronized(lock):
    def sync_with_lock(func):
        def decoit(*args, **kwargs):
            lock.acquire()
            try:
                return func(*args, **kwargs)
            finally:
                lock.release()
        decoit.func_name == func.func_name
        decoit.__doc__ = func.__doc__
        return decoit
    return sync_with_lock

db_lock = threading.Lock()
 
class DB:
    def __init__(self, db_name='./my_test.db'):
        self.db_name = db_name
        self.create_table()
        self.commit()
        self.connect()

    def connect(self):
        self.con = sqlite3.connect(self.db_name, check_same_thread = False)
        self.cur = self.con.cursor()

    @synchronized(db_lock)
    def commit(self):
        self.con.commit()

    @synchronized(db_lock)
    def create_table(self):
        self.connect()
        self.cur.execute('CREATE TABLE if not exists qps_history(view varchar(64), timestamp char(14), qps float)')
        self.cur.execute('CREATE TABLE if not exists nic_rate_history(ethn varchar(64), timestamp char(14), rx_rate float, tx_rate float)')
        self.cur.execute('CREATE TABLE if not exists success_rate_history(view varchar(64), timestamp char(14), success_rate float)')
        self.cur.execute('CREATE TABLE if not exists domaintopn_history(view varchar(64), timestamp char(14), domain varchar(256), query_num int)')
        self.cur.execute('CREATE TABLE if not exists iptopn_history(view varchar(64), timestamp char(14), clientip varchar(128), query_num int)')
        self.cur.execute('CREATE TABLE if not exists rcode_history(view varchar(64), timestamp char(14), rcode varchar(64), query_num int)')
        self.cur.execute('CREATE TABLE if not exists rtype_history(view varchar(64), timestamp char(14), rtype varchar(64), query_num int)')
        self.cur.execute('CREATE TABLE if not exists one_day_qps_history(view varchar(64), timestamp char(14), qps float)')
        self.cur.execute('CREATE TABLE if not exists one_day_nic_rate_history(ethn varchar(64), timestamp char(14), rx_rate float, tx_rate float)')
        self.cur.execute('CREATE TABLE if not exists one_day_success_rate_history(view varchar(64), timestamp char(14), success_rate float)')
        self.cur.execute('CREATE TABLE if not exists one_day_domaintopn_history(view varchar(64), timestamp char(14), domain varchar(256), query_num int)')
        self.cur.execute('CREATE TABLE if not exists one_day_iptopn_history(view varchar(64), timestamp char(14), clientip varchar(128), query_num int)')
        self.cur.execute('CREATE TABLE if not exists one_day_rcode_history(view varchar(64), timestamp char(14), rcode varchar(64), query_num int)')
        self.cur.execute('CREATE TABLE if not exists one_day_rtype_history(view varchar(64), timestamp char(14), rtype varchar(64), query_num int)')

        self.cur.execute('CREATE INDEX if not exists  index_qps on qps_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_suc on success_rate_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_domain on domaintopn_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_ip on iptopn_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_rcode on rcode_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_rtype on rtype_history(view asc, timestamp asc)')

        self.cur.execute('CREATE INDEX if not exists  index_qps_day on one_day_qps_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_suc_day on one_day_success_rate_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_domain_day on one_day_domaintopn_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_ip_day on one_day_iptopn_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_rcode_day on one_day_rcode_history(view asc, timestamp asc)')
        self.cur.execute('CREATE INDEX if not exists  index_rtype_day on one_day_rtype_history(view asc, timestamp asc)')


    @synchronized(db_lock)
    def _insert_value(self, view, d_time, value, v_type):
        self.cur.execute('INSERT INTO "%s_history" VALUES("%s", %s, %f)' % (v_type, view, d_time, float(value)))

    @synchronized(db_lock)
    def _insert_nic_rate(self, ethn, d_time, rx_rate, tx_rate, table_name):
        self.cur.execute('INSERT INTO "%s_history" VALUES("%s", %s, %f, %f)' % (table_name, ethn, d_time, float(rx_rate), float(tx_rate)))

    @synchronized(db_lock)
    def _insert_topn(self, view, d_time, topn_list, topn_type):
        for info in topn_list: 
            self.cur.execute('INSERT INTO "%s_history" VALUES("%s", "%s", "%s", %d)' % (topn_type, view, d_time, info[0], info[1]))

    @synchronized(db_lock)
    def _insert_property(self, view, d_time, r_list, p_type):
        for info in r_list: 
            self.cur.execute('INSERT INTO "%s_history" VALUES("%s", "%s", "%s", %d)' % (p_type, view, d_time, info[0], info[1]))

    @synchronized(db_lock)
    def _get_value(self, view, v_date, v_end, type):
        self.cur.execute('SELECT * FROM "%s_history" where view = \'%s\' and timestamp >= "%s" and timestamp < "%s"' % (type, view, v_date, v_end))
        records = self.cur.fetchall()
        record_len = len(records)
        if record_len <= MAX_POINTS:
            return [(item[1], item[2]) for item in records]
        else:
            li = []
            step = record_len/MAX_POINTS
            for i in range(0, record_len-1, step):
                li.append((records[i][1], records[i][2]))
            return li[0:MAX_POINTS]

    @synchronized(db_lock)
    def _get_nic_rate(self, ethn, v_date, v_end, table_name):
        self.cur.execute('SELECT * FROM "%s_history" where ethn = "%s" and timestamp >= "%s" and timestamp < "%s"' % (table_name, ethn, v_date, v_end))
        records = self.cur.fetchall()
        record_len = len(records)
        if record_len <= MAX_POINTS:
            return [(item[1], item[2], item[3]) for item in records]
        else:
            li = []
            step = record_len/MAX_POINTS
            for i in range(0, record_len-1, step):
                li.append((records[i][1], records[i][2], records[i][3]))
            return li[0:MAX_POINTS]

    @synchronized(db_lock)
    def get_topn(self, view, v_date, v_end, topn_type, topn):
        self.cur.execute('SELECT * FROM "%s_history" where view = \'%s\' and timestamp >= "%s" and timestamp < "%s"' % (topn_type, view, v_date, v_end))
        results = [(item[2], item[3]) for item in self.cur.fetchall()]
        map = defaultdict(int)
        for result in results:
            map[result[0]] += int(result[1])
        sort_r = sorted(map.iteritems(), key=lambda(k,v):v, reverse=True)[0:int(topn)]
        return sort_r;

    @synchronized(db_lock)
    def _get_property(self, view, v_date, v_end, property):
        self.cur.execute('SELECT * FROM "%s_history" where view = \'%s\' and timestamp >= "%s" and timestamp < "%s"' % (property, view, v_date, v_end))
        results = [(item[2], item[3]) for item in self.cur.fetchall()]
        map = defaultdict(int)
        for result in results:
            map[result[0]] += int(result[1])
        sort_r = sorted(map.iteritems(), key=lambda(k,v):v, reverse=True)
        return sort_r;

    def insert_qps(self, view, d_time, rate):
        self._insert_value(view, d_time, rate, 'qps')

    def insert_success_rate(self, view, d_time, rate):
        self._insert_value(view, d_time, rate, 'success_rate')

    def insert_nic_rate(self, ethn, d_time, rx_rate, tx_rate):
        self._insert_nic_rate(ethn, d_time, rx_rate, tx_rate, 'nic_rate')

    def insert_domain_topn(self, view, d_time, domaintopn_list):
        self._insert_topn(view, d_time, domaintopn_list, 'domaintopn')

    def insert_ip_topn(self, view, d_time, iptopn_list):
        self._insert_topn(view, d_time, iptopn_list, 'iptopn')

    def insert_rcode(self, view, d_time, rcode_list):
        self._insert_property(view, d_time, rcode_list, 'rcode')

    def insert_rtype(self, view, d_time, rtype_list):
        self._insert_property(view, d_time, rtype_list, 'rtype')

    def insert_one_day_qps(self, view, d_time, rate):
        self._insert_value(view, d_time, rate, 'one_day_qps')

    def insert_one_day_success_rate(self, view, d_time, rate):
        self._insert_value(view, d_time, rate, 'one_day_success_rate')

    def insert_one_day_nic_rate(self, ethn, d_time, rx_rate, tx_rate):
        self._insert_nic_rate(ethn, d_time, rx_rate, tx_rate, 'one_day_nic_rate')

    def insert_one_day_domain_topn(self, view, d_time, domaintopn_list):
        self._insert_topn(view, d_time, domaintopn_list, 'one_day_domaintopn')

    def insert_one_day_ip_topn(self, view, d_time, iptopn_list):
        self._insert_topn(view, d_time, iptopn_list, 'one_day_iptopn')

    def insert_one_day_rcode(self, view, d_time, rcode_list):
        self._insert_property(view, d_time, rcode_list, 'one_day_rcode')

    def insert_one_day_rtype(self, view, d_time, rtype_list):
        self._insert_property(view, d_time, rtype_list, 'one_day_rtype')

    def _get_time_obj(self, s):
        tt = int(s[0:4]), int(s[4:6]), int(s[6:8]), int(s[8:10]), int(s[10:12]), int(s[12:14]), 0, 0, 0
        return time.mktime(tt)

    def _less_than_some_interval(self, v_date, v_end, interval):
        v_s = self._get_time_obj(v_date)
        v_e = self._get_time_obj(v_end)
        if v_e - v_s < interval: return True
        return False
    def get_nic_rate(self, ethn, v_date, v_end):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self._get_nic_rate(ethn, v_date, v_end, 'nic_rate')
        else:
            return self._get_nic_rate(ethn, v_date, v_end, 'one_day_nic_rate')

    def get_qps(self, view, v_date, v_end):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self._get_value(view, v_date, v_end, 'qps')
        else:
            return self._get_value(view, v_date, v_end, 'one_day_qps')

    def get_success_rate(self, view, v_date, v_end):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self._get_value(view, v_date, v_end, 'success_rate')
        else:
            return self._get_value(view, v_date, v_end, 'one_day_success_rate')

    def get_rcode(self, view, v_date, v_end):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self._get_property(view, v_date, v_end, 'rcode')
        else:
            long_results = self._get_property(view, v_date, v_end, 'one_day_rcode')
            latest_time = self._get_latest_time('one_day_rcode')
            recent_results = self._get_property(view, latest_time, v_end, 'rcode')
            return self._get_union_result(long_results, recent_results)

    def get_rtype(self, view, v_date, v_end):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self._get_property(view, v_date, v_end, 'rtype')
        else:
            long_results = self._get_property(view, v_date, v_end, 'one_day_rtype')
            latest_time = self._get_latest_time('one_day_rtype')
            recent_results = self._get_property(view, latest_time, v_end, 'rtype')
            return self._get_union_result(long_results, recent_results)

    def get_ip_topn(self, view, v_date, v_end, topn):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self.get_topn(view, v_date, v_end, 'iptopn', topn)
        else:
            long_results = self.get_topn(view, v_date, v_end, 'one_day_iptopn', topn)
            latest_time = self._get_latest_time('one_day_iptopn')
            recent_results = self.get_topn(view, latest_time, v_end, 'iptopn', topn)
            return self._get_union_result(long_results, recent_results, topn)

    @synchronized(db_lock)
    def _get_latest_time(self, table):
        self.cur.execute('SELECT max(timestamp) FROM "%s_history"' % (table))
        count = self.cur.fetchone()
        if count[0]:
            return count[0]
        else:
            return "20130101010101"

    def _get_union_result(self, long_results, recent_results, topn=1000):
        map = defaultdict(int)
        for result in long_results:
            map[result[0]] += int(result[1])
        for result in recent_results:
            map[result[0]] += int(result[1])
        sort_r = sorted(map.iteritems(), key=lambda(k,v):v, reverse=True)[0:int(topn)]
        return sort_r;

    def get_domain_topn(self, view, v_date, v_end, topn):
        if self._less_than_some_interval(v_date, v_end, TIME_INTERVAL):
            return self.get_topn(view, v_date, v_end, 'domaintopn', topn)
        else:
            long_results = self.get_topn(view, v_date, v_end, 'one_day_domaintopn', topn)
            latest_time = self._get_latest_time('one_day_domaintopn')
            recent_results = self.get_topn(view, latest_time, v_end, 'domaintopn', topn)
            return self._get_union_result(long_results, recent_results, topn)
    
    @synchronized(db_lock)
    def delete_data_long_ago(self, v_date):
        self.cur.execute('DELETE FROM qps_history where timestamp < %s' % v_date)
        self.cur.execute('DELETE FROM rtype_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM rcode_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM iptopn_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM domaintopn_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM success_rate_history where timestamp < %s' % v_date)
        self.cur.execute('DELETE FROM nic_rate_history where timestamp < %s' % v_date)
        self.con.commit()

    @synchronized(db_lock)
    def delete_data_longlong_ago(self, v_date):
        self.cur.execute('DELETE FROM one_day_qps_history where timestamp < %s' % v_date)
        self.cur.execute('DELETE FROM one_day_rtype_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM one_day_rcode_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM one_day_iptopn_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM one_day_domaintopn_history where timestamp < "%s"' % v_date)
        self.cur.execute('DELETE FROM one_day_success_rate_history where timestamp < %s' % v_date)
        self.cur.execute('DELETE FROM one_day_nic_rate_history where timestamp < %s' % v_date)
        self.con.commit()

    def close(self):
        self.cur.close()
        self.con.close()

if __name__ == '__main__':
    db = DB('./topn_and_qps.db')
    db.insert_domain_topn('default', '20130818000000',[['baidu.com', 100]] )
    db.commit()

cd src
make
cp log_main ../logstats
cp restart_logserver ../logstats
cp loglimit.sh ../logstats
cp logcut.conf ../logstats
cp add_to_cron.sh ../logstats
cd ../webserver
python compile_all.py
cp *.pyc ../logstats
cd ../center
python compile_all.py
cp *.pyc ../logstats
cp restart_center ../logstats
cp nodes.conf ../logstats
cd ..
tar zcvf logstats.tar.gz logstats

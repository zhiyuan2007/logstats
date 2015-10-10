#echo "* */1 * * * logrotate  /usr/local/logstats/logcut.conf > /dev/null 2>&1" >> /var/spool/cron/root
export PATH=/usr/sbin:$PATH
logrotate  /usr/local/logstats/logcut.conf >> /var/log/logstats/loglimit.log 2>&1

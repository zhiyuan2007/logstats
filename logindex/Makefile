OBJ= log_main
all: ${OBJ}
log_main:
	gcc -g -o log_main main.c log_utils.c log_stats.c statsmessage.pb-c.c \
	log_view_tree.c  log_view_stats.c dig_mem_pool.c \
	dig_rb_tree.c dig_radix_tree.c log_config.c \
	log_domain_tree.c \
	-lprotobuf-c -lpthread
install:
	cp log_server /usr/bin
	cp restart_logserver /usr/bin
.PHONY: ${OBJ}

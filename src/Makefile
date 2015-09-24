OBJ= log_main
all: ${OBJ}
log_main:
	gcc -g -o log_main main.c log_topn.c adlist.c statsmessage.pb-c.c log_utils.c zip_socket.c zip_addr.c log_view_tree.c  log_view_stats.c dig_mem_pool.c log_name_tree.c dig_rb_tree.c log_heap.c zip_lru_list.c dig_radix_tree.c -lprotobuf-c -lpthread
install:
	cp log_server /usr/bin
	cp restart_logserver /usr/bin
.PHONY: ${OBJ}

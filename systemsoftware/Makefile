
all: sensor_gateway sensor

sensor_gateway : main.c connmgr.c datamgr.c sensor_db.c sbuffer.c lib/dplist.c lib/tcpsock.c
	gcc main.c connmgr.c datamgr.c sbuffer.c sensor_db.c -Wall -Werror -lm -L./lib -Wl,-rpath=./lib -ltcpsock -ldplist -lpthread -lsqlite3 -DTIMEOUT=5 -DSET_MAX_TEMP=20 -DSET_MIN_TEMP=10 -o sensor_gateway

sensor : sensor_node.c lib/tcpsock.c
	gcc sensor_node.c -L./lib -Wl,-rpath=./lib -ltcpsock -o sensor_node


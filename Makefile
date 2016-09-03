# be sure to export TP_DIR = <thread_pool_c path>
CFLAGS := -O3 -I$(TP_DIR) -fPIC
TCPS_OBJ := tcp_server.o

LIBS = -lpthread $(TP_DIR)/libtpl.a

SLIB = libtcpsrv.a
SHARED_LIB= libtcpsrv.so

CC := gcc 
AR := ar r

TEST_SRC = tcp_server_test.c

all: tcpsrv_lib tcpsrv_test

tcpsrv_lib: $(TCPS_OBJ)
	$(AR) $(SLIB) $(TCPS_OBJ)
	$(CC) -shared -o $(SHARED_LIB) $(TCPS_OBJ)

tcpsrv_test: $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@  $^ $(LIBS) $(SLIB)

clean: 
	rm -f *.o *.a *.so core* tcpsrv_test

thread_pool.o: tcp_server.h

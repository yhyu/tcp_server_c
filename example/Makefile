# be sure to export TP_DIR = <thread_pool_c path>
TCPSRV_DIR = ..
CFLAGS := -O3 -I$(TP_DIR) -I$(TCPSRV_DIR) -fPIC
NCP_S_OBJ := netcopy_s.o
NCP_C_OBJ := netcopy_c.o

LIBS = -lpthread $(TP_DIR)/libtpl.a $(TCPSRV_DIR)/libtcpsrv.a

all: ncp ncpd

ncp: $(NCP_C_OBJ)
	$(CC) $(CFLAGS) -o $@  $^

ncpd: $(NCP_S_OBJ)
	$(CC) $(CFLAGS) -o $@  $^ $(LIBS)

clean: 
	rm -f *.o *.a *.so core* ncp ncpd

netcopy_s.o: netcopy_s.h netcopy.h
netcopy_c.o: netcopy.h

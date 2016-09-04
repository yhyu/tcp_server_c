# tcp_server_c
A library to create tcp server with multiple concurrent connections capability.

## How to use?

Because [thread_pool_c](https://github.com/yhyu/thread_pool_c) is used to handle tcp server jobs, you've to specify its path. For instance, 

```
export TP_DIR=$HOME/workspace/thread_pool_c
make clean;make all
```
Notes: the number of threads have nothing to do with concurrent connections of tcp server, but are related to concurrent running jobs.

### Create a tcp server:

Before you can create tcp server, you've to prepare two handlers, one is new context creation callback function, the other is data receiving callback function. For instance,
```
static void* get_new_context(void* args)
{
    char** rsp_ptr = (char**)args;
    *rsp_ptr = malloc(RECV_BUF_SIZE);
    return *rsp_ptr;
}

static act_state_t echo_handler(int fd, void* ctxt)
{
    if (0 < read(fd, ctxt, RECV_BUF_SIZE))
        return ACT_CONTINUE; // returns ACT_CONTINUE means we've more data to receive.

    free(ctxt);
    close(fd);
    return ACT_CLOSED; // returns ACT_CLOSED means no more data to receive, and we
                       // should clean up resources.
}
```

Just like we me mentioned, we need thread pool to deal with concurrent running jobs. We've to create thread pool in advance.
```
// prepare thread pool
tp_strategy_t strategy = {
    .min = 5,
    .max = 10,
    .inc = 3,
    .des = 3,
    .low = 2
};
tp_context_t* tp_ctxt = create_thread_pool(&strategy);

// create tcp server to listen on all network interfaces
server_ctxt_t* srv_ctxt = create_tcp_server(
    tp_ctxt,        // thread pool
    0,              // 0: non-blocking mode; 1: blocking mode
    "0.0.0.0",      // ip address to listen (here is ALL)
    port,           // listen port
    get_new_context,// new context creation callback, it's called when a new client is
                    // connected. The return value will be passed in data receiving callback.
    echo_handler,   // data receiving callback
    &response);     // any argument that will be passed in new context creation callback.
```
For simple usage, please refer to test program, [tcp_server_test.c](https://github.com/yhyu/tcp_server_c/blob/master/tcp_server_test.c).

## Netcopy example

[Netcpy] (https://github.com/yhyu/tcp_server_c/tree/master/example) example is a server/client example to copy local file to remove. This is just a simple example, and only support single file copy. It might be enhanced to support multiple files and remove to remove copy in future.

Start server (on remove side):
```
nohub ./ncpd -p 12345&
```

Copy local file to remove:
```
./ncp ./test.txt 192.168.101.101:12345/tmp/test.txt
```

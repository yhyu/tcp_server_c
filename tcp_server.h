#pragma once

#include "thread_pool.h"

typedef enum {
    ACT_CONTINUE,
    ACT_CLOSED,
    ACT_TERMINIATE
} act_state_t;

typedef void* (*get_client_ctxt)(void* args);
typedef act_state_t (*event_handler)(int fd, void* ctxt);

typedef struct event_data
{
    int     sync;   // synchronous action
    int     sfd;    // socket fd
    int     efd;    // epoll fd
    event_handler   on_action;  // reaction handler
    void*           act_args;   // handler args
} event_data_t;

typedef struct accept_data
{
    event_data_t    ev_data;
    get_client_ctxt cln_get_ctxt;
    event_handler   cln_action;
    void*           cln_args;
} accept_data_t;

typedef struct server_ctxt
{
    accept_data_t   accept_ctxt;
    tp_context_t*   tp_ctxt;
    pthread_mutex_t mtx;
} server_ctxt_t;

server_ctxt_t* create_tcp_server(
    tp_context_t* tp_ctxt, int sync,
    const char* ip, unsigned short port,
    get_client_ctxt new_client,
    event_handler handler, void* args);

void destroy_tcp_server(server_ctxt_t* srv_ctxt);

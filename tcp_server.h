/*
 * Copyright 2016 Edward Yu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

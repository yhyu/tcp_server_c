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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "tcp_server.h"

#define RECV_BUF_SIZE   128

tp_strategy_t strategy = {
    .min = 5,
    .max = 10,
    .inc = 3,
    .des = 3,
    .low = 2
};

static void* get_new_context(void* args)
{
    char** rsp_ptr = (char**)args;
    *rsp_ptr = malloc(RECV_BUF_SIZE);
    return *rsp_ptr;
}

static act_state_t echo_handler(int fd, void* ctxt)
{
    if (0 < read(fd, ctxt, RECV_BUF_SIZE))
        return ACT_CONTINUE;

    free(ctxt);
    close(fd);
    return ACT_CLOSED;
}

static act_state_t err_handler(int fd, void* ctxt)
{
    free(ctxt);
    close(fd);
    return ACT_CLOSED;
}

void test_basic()
{
    // prepare thread pool for tcp server
    tp_context_t* tp_ctxt = create_thread_pool(&strategy);
    if (!tp_ctxt) {
        printf("%s, Fail: create_thread_pool fail.\n", __func__);
        return;
    }

    // prepare server
    int fd = -1, err = 1;
    const char request[] = "Hello TCP Server!";
    char* response = NULL;
    unsigned short port = 12345;
    server_ctxt_t* srv_ctxt = create_tcp_server(tp_ctxt, 0, 0, "0.0.0.0" , port, get_new_context, echo_handler, err_handler, &response);
    if (!srv_ctxt) {
        printf("%s, Fail: create_tcp_server fail.\n", __func__);
        destroy_thread_pool(tp_ctxt);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // send request
    do {
        if (-1 == (fd = socket(AF_INET, SOCK_STREAM, 0))) {
            printf("%s, Fail: create socket fail, err %d.\n", __func__, errno);
            break;
        }
        if (-1 ==(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)))) {
            printf("%s, Fail: connect server fail, err %d.\n", __func__, errno);
            break;
        }
        if (strlen(request) != write(fd, request, strlen(request))) {
            printf("%s, Fail: send request fail, err %d.\n", __func__, errno);
            break;
        }
        usleep(100);
        if (0 != memcmp(request, response, strlen(request))) {
            printf("%s, Fail: test echo fail, expect \"%s\", actual \"%s\".\n", __func__, request, response);
            break;
        }
        err = 0;
    } while (0);

    if (fd != -1) close(fd);

    // destroy server
    destroy_tcp_server(srv_ctxt);

    // destroy thread pool
    destroy_thread_pool(tp_ctxt);
    if (!err)
        printf("%s, Pass.\n", __func__);
}

int main(int argc, char *argv[])
{
    test_basic();
}
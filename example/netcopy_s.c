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
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "tcp_server.h"
#include "netcopy_s.h"

static void* get_new_context(void* args)
{
    read_ctxt_t* ctxt = malloc(sizeof(read_ctxt_t));
    memset(ctxt, 0, sizeof(read_ctxt_t));
    ctxt->target_fd = -1;
    return ctxt;
}

static act_state_t ncp_handler(int fd, void* ctxt)
{
    ssize_t read_len = 0;
    read_ctxt_t* rctxt = (read_ctxt_t*)ctxt;
    do {
        // received first packet
        if (!rctxt->read_header) {
            // read header
            read_len = read(fd, &rctxt->header, sizeof(ncp_header_t));
            if (read_len < 0 && errno == EAGAIN)
                return ACT_CONTINUE;
            if (read_len != sizeof(ncp_header_t)) {
                perror("read header fail");
                break;
            }
            rctxt->read_header = 1;
        }

        if (!rctxt->read_path) {
            int err = 1, dst_fd = -1;
            char* file_name = malloc(rctxt->header.path_len + 1);
            if (!file_name)
                break;
            do {
                // read file name
                read_len = read(fd, file_name, rctxt->header.path_len);
                if (read_len < 0 && errno == EAGAIN)
                    return ACT_CONTINUE;
                if (read_len != rctxt->header.path_len) {
                    perror("read file name fail");
                    break;
                }
                file_name[rctxt->header.path_len] = 0;
                if (-1 == (dst_fd = open(file_name, O_CREAT|O_TRUNC|O_RDWR, rctxt->header.mode))) {
                    fprintf(stderr, "open file \"%s\" fail, err %d\n", file_name, errno);
                    break;
                }
                if (-1 == lseek(dst_fd, rctxt->header.file_len - 1, SEEK_SET)) {
                    perror("expand file fail");
                    break;
                }
                if (1 != write(dst_fd, "", 1)) {
                    perror("write EOF fail");
                    break;
                }
                rctxt->data = mmap(NULL, rctxt->header.file_len, PROT_WRITE, MAP_SHARED, dst_fd, 0);
                if (MAP_FAILED == rctxt->data) {
                    perror("mmap file fail");
                    rctxt->data = NULL;
                    break;
                }
                err = 0;
                rctxt->target_fd = dst_fd;
                rctxt->read_path = 1;
            } while(0);

            free(file_name);
            if (err) {
                if (dst_fd != -1)
                    close(dst_fd);
                break;
            }
        }

        // received data
        if (rctxt->read_path) {
            read_len = read(fd, rctxt->data + rctxt->rbytes, rctxt->header.file_len - rctxt->rbytes);
            if (read_len > 0) {
                rctxt->rbytes += (uint64_t)read_len;
                if (rctxt->rbytes >= rctxt->header.file_len)
                    break; // read finished
            }
            else if (0 == read_len ||
                     (read_len < 0 && errno != EAGAIN)) {
                perror("read data fail");
                break;
            }
        }

        return ACT_CONTINUE;
    } while(0);

    if (rctxt->data) {
        munmap(rctxt->data, rctxt->header.file_len);
        rctxt->data = NULL;
    }
    if (rctxt->target_fd != -1) {
        close(rctxt->target_fd);
        rctxt->target_fd = -1;
    }
    free(ctxt);
    close(fd);
    return ACT_CLOSED;
}

static act_state_t err_handler(int fd, void* ctxt)
{
    ssize_t read_len = 0;
    read_ctxt_t* rctxt = (read_ctxt_t*)ctxt;

    if (rctxt->data) {
        munmap(rctxt->data, rctxt->header.file_len);
        rctxt->data = NULL;
    }
    if (rctxt->target_fd != -1) {
        close(rctxt->target_fd);
        rctxt->target_fd = -1;
    }
    free(ctxt);
    close(fd);
    return ACT_CLOSED;
}

void start_ncpd(const char* ip, unsigned short port)
{
    tp_strategy_t strategy = {
        .min = 5,
        .max = 20,
        .inc = 3,
        .des = 3,
        .low = 2
    };

    // prepare thread pool for tcp server
    tp_context_t* tp_ctxt = create_thread_pool(&strategy);
    if (!tp_ctxt) {
        printf("%s, Fail: create_thread_pool fail.\n", __func__);
        return;
    }

    // prepare server
    server_ctxt_t* srv_ctxt = create_tcp_server(tp_ctxt, 1, 1, ip , port, get_new_context, ncp_handler, err_handler, NULL);
}

const char usages[] = "\n\tUsage %s -p listen_port [-s server_ip]\n"        \
                      "\t\t-s server_ip     server ip, default all ip.\n"   \
                      "\t\t-p listen_port   server listen port number.\n";

int main(int argc, char *argv[])
{
    const char* server_ip = "0.0.0.0";
    unsigned short port = 0;
    int oc;
    while ((oc = getopt(argc, argv, "s:p:")) != -1) {
        switch(oc) {
            case 's':
                server_ip = optarg;
                break;

            case 'p':              
                port = (unsigned short)atoi(optarg);
                break;

            default:
                printf(usages, argv[0]);
                return 1;
        }
    }
    if (!port) {
        printf(usages, argv[0]);
        return 1;
    }
    start_ncpd(server_ip, port);
}
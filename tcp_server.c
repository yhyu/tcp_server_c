#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "tcp_server.h"

#define MAXEVENTS   20

int set_non_blocking(int fd)
{
    int flags, s;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror ("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl (fd, F_SETFL, flags);
    if (s == -1) {
        perror ("fcntl");
        return -1;
    }
    return 0;
}

static act_state_t on_accept(int fd, void* args)
{
    static int conn_cnt = 0;
    accept_data_t* apt_data = (accept_data_t*)args;
    event_data_t* ev_data = &apt_data->ev_data;
    int cfd = -1;
    while ((cfd = accept(fd, NULL, NULL)) != -1)
    {
        // set client fd non-blocking
        if (0 != set_non_blocking(cfd))
        {
            close(cfd);
            continue;
        }

        // prepare client data
        void* cln_ctxt = NULL;
        if (apt_data->cln_get_ctxt)
            cln_ctxt = apt_data->cln_get_ctxt(apt_data->cln_args);

        event_data_t* cln_ev_data = (event_data_t*)malloc(sizeof(event_data_t));
        memset(cln_ev_data, 0, sizeof(event_data_t));
        cln_ev_data->sfd = cfd;
        cln_ev_data->efd = ev_data->efd;
        cln_ev_data->on_action = apt_data->cln_action;
        cln_ev_data->act_args = cln_ctxt;

        // add the client to waiting queue
        struct epoll_event evt = {
            .events = EPOLLIN | EPOLLET,
            .data.ptr = cln_ev_data
        };
        if (-1 == epoll_ctl(ev_data->efd, EPOLL_CTL_ADD, cfd, &evt))
            perror("epoll_ctl fail");
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK)
        perror("accept fail");

    return ACT_CONTINUE;
}

static void event_worker(void* args)
{
    event_data_t* ev_data = (event_data_t*)args;
    act_state_t state = ev_data->on_action(ev_data->sfd, ev_data->act_args);

    if (ACT_CONTINUE == state) {
        // add it back to waiting queue
        struct epoll_event evt = {
            .events = EPOLLIN | EPOLLET,
            .data.ptr = ev_data
        };
        if (-1 == epoll_ctl(ev_data->efd, EPOLL_CTL_ADD, ev_data->sfd, &evt))
            perror("epoll_ctl fail");
    }
    else if (ACT_CLOSED == state) {
        memset(ev_data, 0, sizeof(event_data_t));
        free(ev_data);
    }
}

static int multiplexer(int efd, tp_context_t* tp_ctxt)
{
    struct epoll_event active_events[MAXEVENTS];
    while (1)
    {
        int ev_num = epoll_wait(efd, active_events, MAXEVENTS, 1000);
        if (-1 == ev_num)
            break;

        int idx = 0;
        for (idx = 0; idx < ev_num; ++idx)
        {
            event_data_t* ev_data = (event_data_t*)active_events[idx].data.ptr;
            if (!ev_data)
            {
                perror("event data is NULL");
                continue;
            }

            // remove from waiting list
            struct epoll_event ev_del = {0};
            epoll_ctl(ev_data->efd, EPOLL_CTL_DEL, ev_data->sfd, &ev_del);

            if ((active_events[idx].events & EPOLLERR) ||
                (active_events[idx].events & EPOLLHUP) ||
                (!(active_events[idx].events & EPOLLIN)))
            {
                perror("epoll error");

                close(ev_data->sfd);
                free(ev_data);
                continue;
            }
            else
            {
                if (ev_data->on_action) {
                    if (ev_data->sync)
                        event_worker(ev_data);
                    else
                        run_job_in_tp(tp_ctxt, event_worker, ev_data);
                }
            }
        }
    }

    return 0;
}

static int start_listen(struct sockaddr* addr, int af, int addr_len)
{
    int err = -1;
    int sfd = socket(af, SOCK_STREAM, 0);
    if (-1 == sfd) {
        perror("create socket failed ");
        return -1;
    }

    do {
        if (0 != set_non_blocking(sfd))
            break;

        if (-1 == (bind(sfd, addr, addr_len))) {
            perror("bind fail");
            break;
        }

        if (-1 == (listen(sfd, SOMAXCONN))) {
            perror("listen fail");
            break;
        }
        err = 0;
    } while(0);

    if (err && -1 != sfd) {
        close(sfd);
        sfd = -1;
    }
    return sfd;
}

static void listen_worker(void* args)
{
    server_ctxt_t* srv_ctxt = (server_ctxt_t*)args;
    pthread_mutex_lock(&srv_ctxt->mtx);
    multiplexer(srv_ctxt->accept_ctxt.ev_data.efd, srv_ctxt->tp_ctxt);
    pthread_mutex_unlock(&srv_ctxt->mtx);
}

server_ctxt_t* create_tcp_server(
    tp_context_t* tp_ctxt, int sync,
    const char* ip, unsigned short port,
    get_client_ctxt new_client,
    event_handler handler, void* args)
{
    int efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create1 fail");
        return NULL;
    }

    int done = 1, sfd = -1;
    server_ctxt_t* srv_ctxt = (server_ctxt_t*)malloc(sizeof(server_ctxt_t));
    accept_data_t* acp_data = &srv_ctxt->accept_ctxt;
    event_data_t* ev_data = &acp_data->ev_data;

    srv_ctxt->tp_ctxt = tp_ctxt;
    pthread_mutex_init(&srv_ctxt->mtx, NULL);

    do {
        struct sockaddr_in host_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port)
        };
        host_addr.sin_addr.s_addr = inet_addr(ip);
        if (-1 == (sfd = start_listen((struct sockaddr*)&host_addr, AF_INET, sizeof(struct sockaddr_in))))
            break;

        ev_data->sync = 1;
        ev_data->sfd = sfd;
        ev_data->efd = efd;
        ev_data->on_action = on_accept;
        ev_data->act_args = acp_data;

        acp_data->cln_get_ctxt = new_client;
        acp_data->cln_action = handler;
        acp_data->cln_args = args;

        struct epoll_event evt = {
            .events = EPOLLIN | EPOLLET,
            .data.ptr = ev_data
        };
        if (-1 == epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &evt)) {
            perror("epoll_ctl fail");
            break;
        }

        if (sync) {
            multiplexer(efd, tp_ctxt);
            break;
        }

        if (0 == run_job_in_tp(tp_ctxt, listen_worker, srv_ctxt))
            done = 0;
    } while(0);

    if (done) {
        if (sfd != -1) close(sfd);
        if (efd != -1) close(efd);
        if (srv_ctxt) {
            pthread_mutex_destroy(&srv_ctxt->mtx);
            free(srv_ctxt);
        }
        return NULL;
    }
    return srv_ctxt;
}

void destroy_tcp_server(server_ctxt_t* srv_ctxt)
{
    accept_data_t* acp_data = &srv_ctxt->accept_ctxt;
    event_data_t* ev_data = &acp_data->ev_data;

    close(ev_data->sfd);
    close(ev_data->efd);
    // just make sure multiplexer done
    pthread_mutex_lock(&srv_ctxt->mtx);
    pthread_mutex_unlock(&srv_ctxt->mtx);
    pthread_mutex_destroy(&srv_ctxt->mtx);
    free(srv_ctxt);
}

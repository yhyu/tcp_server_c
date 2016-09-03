#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "netcopy.h"

const char usages[] = "\n\tUsage %s source target\n"        \
                      "\t\tsource   local file name.\n"   \
                      "\t\ttarget   target file name, format: ip:port/<path>.\n";

static int connect_to(const char* ip, unsigned short port)
{
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    server_addr.sin_addr.s_addr = inet_addr(ip);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd) {
        perror("create socket fail");
        return -1;
    }
    if (-1 ==(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)))) {
        fprintf(stderr, "connect to %s:%d fail, err %d.\n", ip, port, errno);
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf(usages, argv[0]);
        return -1;
    }

    const char* source = argv[1];
    char *tmp = NULL, *ip = argv[2];
    if (NULL == (tmp = strchr(ip, ':'))) {
        printf(usages, argv[0]);
        return -2;
    }
    *tmp = '\0';

    char* port_str = ++tmp;
    if (NULL == (tmp = strchr(port_str, '/'))) {
        printf(usages, argv[0]);
        return -3;
    }
    *tmp = '\0';
    int port = atoi(port_str);
    *tmp = '/';

    // open source file
    struct stat src_stat = {0};
    int src_fd = open(source, O_RDONLY|O_CLOEXEC);
    if (-1 == src_fd) {
        fprintf(stderr, "open file \"%s\" fail, err %d\n", source, errno);
        return -4;
    }
    fstat(src_fd, &src_stat);

    // connect to server
    int server_fd = connect_to(ip, (unsigned short)port);
    if (-1 == server_fd) {
        perror("connect fail");
        return -5;
    }

    // write header
    int file_name_len = strlen(tmp);
    ncp_header_t header = {
        .path_len = file_name_len,
        .mode = src_stat.st_mode,
        .file_len = src_stat.st_size
    };
    if (sizeof(header) != write(server_fd, &header, sizeof(header))) {
        perror("send header fail");
        return -6;
    }

    // write target file name
    if (file_name_len != write(server_fd, tmp, file_name_len)) {
        perror("send file name fail");
        return -7;
    }

    // send data
    if (src_stat.st_size != sendfile(server_fd, src_fd, NULL, src_stat.st_size)) {
        perror("send file data fail");
        return -8;
    }
    close(server_fd);
    close(src_fd);
    return 0;
}

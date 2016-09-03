#pragma once

#include "netcopy.h"

typedef struct read_ctxt
{
    ncp_header_t header;
    int         target_fd;      // target file fd
    uint64_t    rbytes;         // read #of bytes
    void*       data;
    uint8_t     read_header;    // header is read
    uint8_t     read_path;      // file path is read
} read_ctxt_t;

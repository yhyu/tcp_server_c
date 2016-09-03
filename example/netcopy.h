#pragma once

typedef struct ncp_header
{
    uint16_t    path_len;   // target file name length
    uint16_t    mode;       // file mode
    uint32_t    reserve2;
    uint64_t    file_len;
} ncp_header_t;
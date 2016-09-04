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

/*
 * Copyright (c) 2016, GNSS Sensor Ltd.
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

#include <vector>
#include <inttypes.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <process.h>

typedef void *_thread_arg_t;
typedef void (*_thread_entry_t)(_thread_arg_t arg1,
							  _thread_arg_t arg2,
							  _thread_arg_t arg3);

typedef void (*_thread_entry_asm)(_thread_entry_t pEntry,
					_thread_arg_t parameter1,
					_thread_arg_t parameter2,
					_thread_arg_t parameter3);

struct ThreadDataType {
    int idx;
    uint64_t stack_offset;
    _thread_entry_asm func;
    _thread_entry_t entry;
    _thread_arg_t args[3];
    int priority;
    unsigned options;
    HANDLE hThread;
    HANDLE hEvent;
    bool preemptive;
};

struct tcs_simple {
    void *link;
    uint64_t flags;
    uint32_t intlock;
    uint32_t rsrv1;
    uint64_t regs[32];
};
enum ERegs {REG_RA=0, REG_V0=15, REG_A0=17, REG_A1=18, REG_A2=19, REG_A3=20, REG_A4=21};


extern "C" int LIBH_create_thread(char *pStackMem,
                                unsigned stackSize, 
                                int priority, 
                                unsigned options);
extern "C" void LIBH_swap(uint64_t stack);

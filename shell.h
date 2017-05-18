/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef __SHELL_H__
#define __SHELL_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "shell_cfg.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @brief Macro to set console buffer size. */

/*! @brief data structure for command table. */
typedef struct shell_cmd_tbl_struct shell_cmd_tbl_t;
typedef struct shell_cmd_tbl_struct* shell_cmd_tbl_ptr_t;
/*! @brief data structure for shell environment. */
typedef struct shell_context_struct  shell_context_t;
typedef struct shell_context_struct* shell_context_ptr_t;
/*! @brief user command function prototype. */
typedef int32_t (* cmd_function_t)(int32_t argc, char** argv);
/*! @brief shell IO operation. */
typedef struct shell_ops_struct  shell_ops_t;
typedef struct shell_ops_struct* shell_ops_ptr_t;


/*! @brief A type for the handle special key. */
enum fun_key_status
{
    SHELL_NORMAL_KEY,
    SHELL_SPEC_KEY,
    SHELL_FUNC_KEY,
};

/*! @brief shell IO user callback type. */
struct shell_ops_struct
{
    void        (*send_data)(const uint8_t *buf, uint32_t len);
    uint32_t    (*rev_data)(uint8_t* buf, uint32_t len, uint32_t timeout);
};

#if 0
struct shell_user_config_struct
{
    char *prompt;
#if defined(SHELL_USE_FILE_STREAM)
    FILE *STDOUT, *STDIN, *STDERR;
#else
    shell_ops_t ops;                /* IO interface operation */
#endif
    shell_cmd_tbl_ptr_t cmd_tab;        /* command tables */
    uint8_t cmd_num;                /* number of user commands */
};
#endif

/*!
 * @brief Runtime state structure for shell.
 *
 * This structure holds data that is used by the shell to manage operation.
 * The user must pass the memory for this run-time state structure and the shell
 * fills out the members.
 */
struct shell_context_struct
{
    const char *prompt;                   /* prompt string */
    enum fun_key_status stat;       /* special key status */
    char    line[SHELL_CONFIG_CBSIZE];   /* consult buffer */
    uint8_t cmd_num;                /* number of user commands */
    uint8_t l_pos;                  /* total line postion */
    uint8_t c_pos;                  /* current line postion */
    shell_cmd_tbl_ptr_t cmd_tbl;        /* command tables */
    bool exit;
};

/*!
 * @brief shell command table struct.
 *
 * This structure holds command function information that is used by the shell to find and compare input command.
 */
struct shell_cmd_tbl_struct
{
    char                *name;
    int32_t             maxargs;
    cmd_function_t      cmd;
};

int32_t shell_get_cmd_exec_flag(void);
int32_t shell_exec_fun(void);
int32_t shell_getc(uint8_t in_ch);
int32_t shell_init(shell_cmd_tbl_ptr_t cmd_tbl,
                   uint32_t cmd_num, const char *prompt);
int32_t shell_get_exit_signal(void);
void shell_signal_exit(void);

#endif /* __SHELL_H__ */

/*******************************************************************************
 * EOF
 ******************************************************************************/


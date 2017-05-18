/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
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

/*
POSIX getopt for Windows
Code given out at the 1985 UNIFORUM conference in Dallas.  
*/

// From std-unix@ut-sally.UUCP (Moderator, John Quarterman) Sun Nov  3 14:34:15 1985
// Relay-Version: version B 2.10.3 4.3bsd-beta 6/6/85; site gatech.CSNET
// Posting-Version: version B 2.10.2 9/18/84; site ut-sally.UUCP
// Path: gatech!akgua!mhuxv!mhuxt!mhuxr!ulysses!allegra!mit-eddie!genrad!panda!talcott!harvard!seismo!ut-sally!std-unix
// From: std-unix@ut-sally.UUCP (Moderator, John Quarterman)
// Newsgroups: mod.std.unix
// Subject: public domain AT&T getopt source
// Message-ID: <3352@ut-sally.UUCP>
// Date: 3 Nov 85 19:34:15 GMT
// Date-Received: 4 Nov 85 12:25:09 GMT
// Organization: IEEE/P1003 Portable Operating System Environment Committee
// Lines: 91
// Approved: jsq@ut-sally.UUCP

// Here's something you've all been waiting for:  the AT&T public domain
// source for getopt(3).  It is the code which was given out at the 1985
// UNIFORUM conference in Dallas.  I obtained it by electronic mail
// directly from AT&T.  The people there assure me that it is indeed
// in the public domain.

// There is no manual page.  That is because the one they gave out at
// UNIFORUM was slightly different from the current System V Release 2
// manual page.  The difference apparently involved a note about the
// famous rules 5 and 6, recommending using white space between an option
// and its first argument, and not grouping options that have arguments.
// Getopt itself is currently lenient about both of these things White
// space is allowed, but not mandatory, and the last option in a group can
// have an argument.  That particular version of the man page evidently
// has no official existence, and my source at AT&T did not send a copy.
// The current SVR2 man page reflects the actual behavor of this getopt.
// However, I am not about to post a copy of anything licensed by AT&T.


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "shell_cfg.h"
#include "shell.h"

/*******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
static shell_context_t gshell_context;
static int32_t cmd_exec_flag = 0;

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define KEY_ESC   (0x1B)
#define KET_DEL   (0x7F)
/*******************************************************************************
 * Code
 ******************************************************************************/
int32_t shell_get_cmd_exec_flag(void)
{
    return cmd_exec_flag;
}

int32_t shell_getc(uint8_t ch)
{
    shell_context_ptr_t context = &gshell_context;

    /* special key */
    if (ch == KEY_ESC)
    {
        context->stat = SHELL_SPEC_KEY;
        return 0;
    }
    else if (context->stat == SHELL_SPEC_KEY)
    {
        /* func key */
        if (ch == '[')
        {
            context->stat = SHELL_FUNC_KEY;
            return 0;
        }

        context->stat = SHELL_NORMAL_KEY;
    }
    else if (context->stat == SHELL_FUNC_KEY)
    {
        context->stat = SHELL_NORMAL_KEY;
        switch(ch)
        {
            /* history operation here */
            case 'A': /* up key */
                break;
            case 'B': /* down key */
                break;
            case 'D': /* left key */
                if (context->c_pos)
                {
                    putchar('\b');
                    context->c_pos --;
                }
                break;
            case 'C': /* right key */
                if (context->c_pos < context->l_pos)
                {
                    printf("%c", context->line[context->c_pos]);
                    context->c_pos ++;
                }
                break;
        }
        return 0;
    }    
    /* handle tab key */
    else if (ch == '\t')
        return 0;
    /* handle backspace key */
    else if (ch == KET_DEL || ch == '\b')
    {
        /* there must be at lastest one char */
        if (context->c_pos == 0)
            return 0;
                
        context->l_pos--;
        context->c_pos--;

        if (context->l_pos > context->c_pos)
        {
            int i;
            memmove(&context->line[context->c_pos],&context->line[context->c_pos + 1],context->l_pos - context->c_pos);
            context->line[context->l_pos] = 0;
            printf("\b%s  \b", &context->line[context->c_pos]);

            /* reset postion */
            for (i = context->c_pos; i <= context->l_pos; i++)
                putchar('\b');
        }
        else /* normal backspace operation */
        {
            printf("\b \b");
            context->line[context->l_pos] = 0;
        }
        return 0;
    }
        
    /* input too long */
    if (context->l_pos >= SHELL_CONFIG_CBSIZE)
        context->l_pos = 0;
        
    /* handle end of line, break */
    if (ch == '\r' || ch == '\n')
    {
        printf("\r\n");
        cmd_exec_flag = 1;
        return 0;
    }
        
    /* normal character */
    if (context->c_pos < context->l_pos)
    {
        int i;

        memmove(&context->line[context->c_pos + 1], &context->line[context->c_pos],
                context->l_pos - context->c_pos); 
        context->line[context->c_pos] = ch;
        printf("%s", &context->line[context->c_pos]);
        /* move the cursor to new position */
        for (i = context->c_pos; i < context->l_pos; i++)
            putchar('\b');
    }
    else
    {
        context->line[context->l_pos] = ch;
        printf("%c", ch);
    }

    ch = 0;
    context->l_pos++;
    context->c_pos++;

    return 0;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : shell_parse_line
 * Description   : Split command strings.
 *
 *END**************************************************************************/
static int shell_parse_line(char *cmd, uint32_t len,
                                  char *argv[SHELL_CONFIG_MAXARGS])
{
    uint32_t argc;
    char *p;
    uint32_t position;

    /* init params */
    p = cmd;
    position = 0;
    argc = 0;

    while (position < len)
    {
        /* skip all banks */
        while ((isblank(*p)) && (position < len))
        {
            *p = '\0';
            p++;
            position++;
        }
        /* process begin of a string */
        if (*p == '"')
        {
            p ++;
            position ++;
            argv[argc] = p; argc ++;
            /* skip this string */
            while (*p != '"' && position < len)
            {
                p ++;
                position ++;
            }
            /* skip '"' */
            *p = '\0';
            p++;
            position++;
        }
        else /* normal char */
        {
            argv[argc] = p;
            argc++;
            while ((*p != ' ' && *p != '\t') && position < len)
            {
                p++;
                position++;
            }
        }
    }
    return argc;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : shell_find_cmd
 * Description   : Split command strings.
 *
 *END**************************************************************************/
static cmd_function_t shell_find_cmd(const char *cmd)
{
    int32_t i;
    int32_t len = strlen(cmd);
    shell_context_ptr_t context = &gshell_context;

    /* param check */
    if((!cmd) || (!len) || (!context))
        return NULL;

    /* find cmd */
    for(i = 0; i < context->cmd_num; i++)
    {
        if(!context->cmd_tbl[i].name)
            break;
        if(!strncmp(cmd, context->cmd_tbl[i].name, len))
            return context->cmd_tbl[i].cmd;
    }
    return NULL;
}

/*FUNCTION**********************************************************************
 *
 * Function Name : shell_exec_fun
 * Description   : Find and execute commands.
 *
 *END**************************************************************************/
int32_t shell_exec_fun(void)
{
    int32_t argc;
    char *argv[SHELL_CONFIG_CBSIZE];
    shell_context_ptr_t context = &gshell_context;
    int32_t ret = 0;
    cmd_function_t func = NULL;

    cmd_exec_flag = 0;

    argc = shell_parse_line(context->line, context->l_pos, argv);
    if (argc == 0)
    {
        ret = -1;
        goto out_reset;
    }

    /* find cmd */
    func = shell_find_cmd(argv[0]);
    if (!func) 
    {
        printf("%s:", argv[0]);
        printf("command not found.\r\n");
        ret = -1;
        goto out_reset;
    }
    /* exec this command */
    ret = func(argc, argv);

out_reset:
    /* reset all params */
    context->c_pos = context->l_pos = 0;
    printf(context->prompt);
    memset(context->line, 0, sizeof(context->line));

    return ret;
}

int32_t shell_get_exit_signal(void)
{
    return gshell_context.exit;
}

void shell_signal_exit(void)
{
    gshell_context.exit = 1;
}

int32_t shell_init(shell_cmd_tbl_ptr_t cmd_tbl,
                         uint32_t cmd_num, const char *prompt)
{
    shell_context_ptr_t context = &gshell_context;

    context->prompt  = prompt;
    context->cmd_tbl = cmd_tbl;
    context->cmd_num = cmd_num;

    printf(context->prompt);
    
    return 0;
}

/*******************************************************************************
 * EOF
 ******************************************************************************/

/*******************************************************************************
* File Name : console.c
*
* Layer 2 - function. Line editor + whitespace tokenizer + table dispatch.
*******************************************************************************/
#include "console.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "board.h"

static const console_cmd_t *table;
static uint32_t             table_len;
static char                 line[CONSOLE_LINE_MAX];
static uint32_t             len;

static void print_help(void)
{
    printf("OK commands:\r\n  help\r\n");
    for (uint32_t i = 0U; i < table_len; i++)
    {
        printf("  %s\r\n", table[i].usage);
    }
}

static void dispatch(char *buf)
{
    char *argv[CONSOLE_ARGS_MAX];
    int   argc = 0;
    char *tok  = strtok(buf, " \t");

    while ((tok != NULL) && (argc < (int)CONSOLE_ARGS_MAX))
    {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    if (argc == 0)
    {
        return;
    }
    if (strcmp(argv[0], "help") == 0)
    {
        print_help();
        return;
    }
    for (uint32_t i = 0U; i < table_len; i++)
    {
        if (strcmp(argv[0], table[i].name) == 0)
        {
            table[i].fn(argc, argv);
            return;
        }
    }
    printf("ERR unknown command '%s' (try help)\r\n", argv[0]);
}

void console_init(const console_cmd_t *cmds, uint32_t count)
{
    table     = cmds;
    table_len = count;
    len       = 0U;
}

void console_poll(void)
{
    char c;

    while (board_console_getc(&c))
    {
        if ((c == '\r') || (c == '\n'))
        {
            if (len > 0U)
            {
                printf("\r\n");
                line[len] = '\0';
                len = 0U;
                dispatch(line);
            }
        }
        else if ((c == '\b') || (c == 0x7F))
        {
            if (len > 0U)
            {
                len--;
                printf("\b \b");
            }
        }
        else if ((len < (CONSOLE_LINE_MAX - 1U)) && (c >= ' '))
        {
            line[len++] = c;
            putchar(c);
        }
    }
    fflush(stdout);
}

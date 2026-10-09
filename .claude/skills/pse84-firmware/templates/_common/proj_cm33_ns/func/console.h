/*******************************************************************************
* File Name : console.h
*
* Layer 2 - function. Line-based command console on the board UART.
* The application supplies a command table; console_poll() reads pending
* bytes (non-blocking), echoes them and dispatches complete lines.
* Built-in command: "help".
*
* Output protocol (keep stable - tests parse it):
*   success  "OK ..."      failure  "ERR <reason>"
*   events   "EVT ..."     state    "<TAG> key=value ..."
*******************************************************************************/
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#define CONSOLE_LINE_MAX    (64U)
#define CONSOLE_ARGS_MAX    (6U)

typedef void (*console_handler_t)(int argc, char *argv[]);

typedef struct
{
    const char        *name;
    const char        *usage;
    console_handler_t  fn;
} console_cmd_t;

void console_init(const console_cmd_t *cmds, uint32_t count);
void console_poll(void);

#endif /* CONSOLE_H */

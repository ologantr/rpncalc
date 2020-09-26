/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020, Paolo Giorgianni <pdg@ologantr.xyz>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions ad the following disclaimer in the documentation
 *    and/or other mateials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define STDIN_BUF_SIZE 1024
#define STACK_NODE_ELEMENTS_NUM 10

enum rpn_op { SUM = 0, SUB, MUL, DIV };
enum rpn_type { DOUBLE = 0, OP, DROP, CLEAR, EXIT };

struct
{
        char cmd[10];
        enum rpn_type cmd_type;
} rpn_commands[] = { { "quit", EXIT },
                     { "drop", DROP },
                     { "clear", CLEAR } };

#define NCMD (int) (sizeof(rpn_commands) / sizeof(rpn_commands[0]))

struct rpn_cmd
{
        union
        {
                double val;
                enum rpn_op op;
        } data;
        int op_times;
        enum rpn_type t;
};

struct stack_node
{
        int ptr;
        double val[STACK_NODE_ELEMENTS_NUM];
        struct stack_node *prev;
        struct stack_node *next;
};

struct stack
{
        struct stack_node *first;
        struct stack_node *last;
        int count;
};

static struct stack_node* stack_alloc_node(void)
{
        struct stack_node *n;

        if ((n = malloc(sizeof(struct stack_node))) == NULL)
        {
                puts("malloc failed");
                exit(-1);
        }

        return n;
}

/* heap-allocated, free it after */
static struct stack* stack_init(void)
{
        struct stack *s;
        struct stack_node *n = stack_alloc_node();

        if ((s = malloc(sizeof(struct stack))) == NULL)
        {
                puts("malloc failed");
                exit(-1);
        }

        n->prev = NULL;
        n->next = NULL;

        /* empty stack type - ptr is the index of the first empty spot
         * also this is the number of elements in the node */
        n->ptr = 0;

        s->first = n;
        s->last = n;
        s->count = 0;

        return s;
}

static void stack_push(struct stack *s, double k)
{
        if (s->last->ptr == STACK_NODE_ELEMENTS_NUM)
        {
                /* the last node is full, allocate a new one */
                struct stack_node *n = stack_alloc_node();
                n->ptr = 0;
                n->prev = s->last;
                n->next = NULL;
                s->last->next = n;
                s->last = n;
        }

        s->last->val[(s->last->ptr)++] = k;
        ++(s->count);
        return;
}

static int stack_pop(struct stack *s, double *ret)
{
        if (s->count == 0) return 0;

        if (s->last->ptr == 0)
        {
                /* the last node is empty, free it and look to the previous one */
                struct stack_node *del = s->last;

                s->last->prev->next = NULL;
                s->last = s->last->prev;
                free(del);
        }

        --(s->count);

        if (ret == NULL)
                /* no need to save the value */
                --(s->last->ptr);
        else
                *ret = s->last->val[--(s->last->ptr)];

        return 1;
}

static void stack_clear(struct stack *s)
{
        /* must free every node other than the first */
        struct stack_node *ptr_1 = s->first->next, *ptr_2 = NULL;

        if (ptr_1 != NULL)
        {
                ptr_2 = ptr_1->next;

                while (ptr_2 != NULL)
                {
                        free(ptr_1);
                        ptr_1 = ptr_2;
                        ptr_2 = ptr_2->next;
                }
        }

        s->first->ptr = 0;
        s->first->next = NULL;
}

static void stack_destroy(struct stack *s)
{
        struct stack_node *ptr_1 = s->first, *ptr_2 = s->first->next;

        while (ptr_2 != NULL)
        {
                free(ptr_1);
                ptr_1 = ptr_2;
                ptr_2 = ptr_2->next;
        }

        free(s);
}

static void stack_print(struct stack *s)
{
        for (struct stack_node *ptr = s->first; ptr != NULL; ptr = ptr->next)
                for (int i = 0; i < ptr->ptr; ++i)
                        printf("%f\n", ptr->val[i]);
}

static void exec_op(struct stack *s, enum rpn_op op, int times)
{
        double x, y, res;

        if (times == 0)
                /* apply the op for all elements */
                times = s->count - 1;

        for (int i = 0; i < times; ++i)
        {
                if (s->count <= 1) return;

                /* check for empty stack */
                if(!stack_pop(s, &res))
                        return;
                x = res;

                if(!stack_pop(s, &res))
                        return;
                y = res;

                switch(op)
                {
                case SUM:
                        res = x + y;
                        break;
                case SUB:
                        res = y - x;
                        break;
                case MUL:
                        res = x * y;
                        break;
                case DIV:
                        if (x == 0)
                        {
                                puts("error - division by zero");
                                return;
                        }

                        res = y / x;
                        break;
                }

                stack_push(s, res);
        }
}

static void cmd_op(struct rpn_cmd *cmd, int times, char f_char_buf)
{
        cmd->t = OP;
        cmd->op_times = times;

        switch(f_char_buf)
        {
        case '+':
                cmd->data.op = SUM;
                break;
        case '-':
                cmd->data.op = SUB;
                break;
        case '*':
                cmd->data.op = MUL;
                break;
        case '/':
                cmd->data.op = DIV;
                break;
        }
}

static void cmd_double(struct rpn_cmd *cmd, char *buf)
{
        cmd->t = DOUBLE;
        cmd->data.val = strtod(buf, NULL);
}

static int is_valid_double(char *buf, int buf_len)
{
        for (int i = 0, p = 0; i < buf_len; ++i)
        {
                if (buf[i] == '.' && !p)
                {
                        /* there is a dot, should be no others */
                        p = 1;
                        continue;
                }

                if (!isdigit(buf[i]))
                        return 0;
        }

        return 1;
}

static int is_valid_int(char *buf, int buf_len)
{
        for (int i = 0; i < buf_len; ++i)
                if(!isdigit(buf[i]))
                        return 0;
        return 1;
}

static int parse_buf(char *buf, struct rpn_cmd *cmd)
{
        int buf_len = (int) strlen(buf);

        /* the user pressed only enter */
        if (!buf_len)
                return -1;

        /* check for a command like drop and quit */
        for (int i = 0; i < NCMD; ++i)
        {
                if (strncmp(buf, rpn_commands[i].cmd, STDIN_BUF_SIZE) == 0)
                {
                        cmd->t = rpn_commands[i].cmd_type;
                        return 0;
                }
        }

        /* check for both an op and a signed number */
        if (*buf == '+' || *buf == '-')
        {
                if (buf_len > 1)
                {
                        /* could be a positive/negative number
                         * prefixed with sign, check the input */
                        if (is_valid_double(buf + 1, buf_len - 1))
                                cmd_double(cmd, buf);
                        else
                                return -1;
                }
                else
                {
                        /* this is an op */
                        cmd_op(cmd, 1, *buf);
                }
        }

        /* check for the remaining ops */
        else if (*buf == '*' || *buf == '/')
        {
                /* check for non-valid things like *3 */
                if (buf_len > 1)
                        return -1;

                /* here we are sure that this is an op */
                cmd_op(cmd, 1, *buf);
        }

        else
        {
                int last_char_index = buf_len - 1, times;
                /* here could be a number or a multiple command like 3+, 4- */
                if (is_valid_double(buf, last_char_index + 1))
                        /* this is a double */
                        cmd_double(cmd, buf);
                /* if the last char is an op and this is an int,
                 * it is a multiple command */
                else if ((buf[last_char_index] == '+' ||
                          buf[last_char_index] == '-' ||
                          buf[last_char_index] == '*' ||
                          buf[last_char_index] == '/') &&
                         /* strlen(buf) - 1 so we do not consider
                          * the final op */
                         is_valid_int(buf, last_char_index))
                {
                        /* we have a multiple op */
                        times = (int) strtol(buf, NULL, 10);
                        cmd_op(cmd, times, buf[last_char_index]);
                }

        }

        return 0;
}

int main(void)
{
        struct stack *s = stack_init();
        char buf[STDIN_BUF_SIZE];
        int retval;
        struct rpn_cmd cmd;

        for (;;)
        {
                printf("> ");

                /* avoid EOF crazy things */
                if (fgets(buf, 1024, stdin) == NULL)
                {
                        putchar('\n');
                        stack_destroy(s);
                        return 0;
                }

                /* remove trailing newline */
                buf[strlen(buf) - 1] = 0;

                retval = parse_buf(buf, &cmd);

                if (retval == -1)
                        goto end;

                switch(cmd.t)
                {
                case DOUBLE:
                        stack_push(s, cmd.data.val);
                        break;
                case OP:
                        if (cmd.op_times == 0)
                                exec_op(s, cmd.data.op, s->count - 1);
                        else
                                exec_op(s, cmd.data.op, cmd.op_times);
                        break;
                case DROP:
                        stack_pop(s, NULL);
                        break;
                case CLEAR:
                        stack_clear(s);
                        break;
                case EXIT:
                        stack_destroy(s);
                        return 0;
                }
        end:
                stack_print(s);
                *buf = 0;
        }
}

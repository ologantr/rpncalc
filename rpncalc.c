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
enum rpn_type { DOUBLE = 0, OP, DROP, EXIT };

struct
{
        char cmd[10];
        enum rpn_type cmd_type;
} rpn_commands[] = { { "quit", EXIT },
                     { "drop", DROP } };

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

static void stack_insert(struct stack *s, double k)
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

static void stack_pop(struct stack *s, double *ret)
{
        if (s->count == 0) return;

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
        if (s->count <= 1 || times == 0) return;

        for (int i = 0; i < times; ++i)
        {
                stack_pop(s, &res);
                x = res;
                stack_pop(s, &res);
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

                stack_insert(s, res);
        }
}

static int parse_buf(char *buf, struct rpn_cmd *cmd)
{
        if (*buf == 0)
                return -1;

        for (int i = 0; i < NCMD; ++i)
        {
                if (strncmp(buf, rpn_commands[i].cmd, STDIN_BUF_SIZE) == 0)
                {
                        cmd->t = rpn_commands[i].cmd_type;
                        return 0;
                }
        }

        /* valid inputs: +, -, *, / and a number */
        if (*buf == '+' | *buf == '-' | *buf == '*' | *buf == '/')
        {
                int times = 1;

                /* OP could be in the form:
                 * + for a single plus, or +3 for 3 adds in sequence */
                if (strlen(buf) > 1)
                {
                        for (int i = 1; i < (int) strlen(buf); ++i)
                                if (!isdigit(buf[i]))
                                        return -1;

                        times = (int) strtol(buf + 1, NULL, 10);
                }

                cmd->t = OP;
                cmd->op_times = times;

                switch(*buf)
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

                return 0;
        }

        else
        {
                for (int i = 0, p = 0; i < (int) strlen(buf); ++i)
                {
                        if (buf[i] == '.' && !p)
                        {
                                /* there is a dot, should be no others */
                                p = 1;
                                continue;
                        }

                        if (!isdigit(buf[i]))
                                return -1;
                }

                cmd->t = DOUBLE;
                cmd->data.val = strtod(buf, NULL);
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
                        stack_insert(s, cmd.data.val);
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
                case EXIT:
                        stack_destroy(s);
                        return 0;
                }
        end:
                stack_print(s);
                *buf = 0;
        }
}

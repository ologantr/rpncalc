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

struct rpn_obj
{
        union
        {
                double val;
                enum rpn_op op;
        } data;
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

static double* stack_pop(struct stack *s)
{
        if (s->count == 0) return NULL;

        if (s->last->ptr == 0)
        {
                /* the last node is empty, free it and look to the previous one */
                s->last->prev->next = NULL;
                s->last = s->last->prev;
                free(s->last);
        }

        --(s->count);
        return &(s->last->val[--(s->last->ptr)]);
}

static void stack_destroy(struct stack *s)
{
        struct stack_node *ptr = s->first;

        while (ptr != NULL)
        {
                free(ptr);
                ptr = ptr->next;
        }

        free(s);
        return;
}

static void stack_print(struct stack *s)
{
        for (struct stack_node *ptr = s->first; ptr != NULL; ptr = ptr->next)
                for (int i = 0; i < ptr->ptr; ++i)
                        printf("%f\n", ptr->val[i]);
}

static void exec_op(struct stack *s, enum rpn_op op)
{
        double x, y, res, *tmp;
        if (s->count <= 1) return;

        tmp = stack_pop(s);
        x = *tmp;
        tmp = stack_pop(s);
        y = *tmp;

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
                res = y / x;
                break;
        }

        stack_insert(s, res);
}

static int parse_buf(char *buf, struct rpn_obj *obj)
{
        if (*buf == 0)
                return -1;

        else if (strncmp(buf, "quit", STDIN_BUF_SIZE) == 0)
        {
                obj->t = EXIT;
                return 0;
        }

        else if (strncmp(buf, "drop", STDIN_BUF_SIZE) == 0)
        {
                obj->t = DROP;
                return 0;
        }

        /* valid inputs: +, -, *, / and a number */
        else if (*buf == '+' | *buf == '-' | *buf == '*' | *buf == '/')
        {
                /* things like +123, -34-5 are not valid */
                if (strlen(buf) > 1)
                        return -1;

                obj->t = OP;
                switch(*buf)
                {
                case '+':
                        obj->data.op = SUM;
                        break;
                case '-':
                        obj->data.op = SUB;
                        break;
                case '*':
                        obj->data.op = MUL;
                        break;
                case '/':
                        obj->data.op = DIV;
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

                obj->t = DOUBLE;
                obj->data.val = strtod(buf, NULL);
        }

        return 0;
}

int main(void)
{
        struct stack *s = stack_init();
        char buf[STDIN_BUF_SIZE];
        int retval;
        struct rpn_obj obj;

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

                retval = parse_buf(buf, &obj);

                if (retval == -1)
                        goto end;

                switch(obj.t)
                {
                case DOUBLE:
                        stack_insert(s, obj.data.val);
                        break;
                case OP:
                        exec_op(s, obj.data.op);
                        break;
                case DROP:
                        stack_pop(s);
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

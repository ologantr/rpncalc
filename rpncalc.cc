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

#include <list>
#include <memory>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <string>
#include <string.h>

enum rpn_op { SUM = 0, SUB, MUL, DIV };
enum rpn_type { DOUBLE = 0, OP, DROP, CLEAR };

struct
{
        char cmd[10];
        enum rpn_type cmd_type;
} rpn_commands[] = { { "drop", DROP },
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

static void exec_op(std::unique_ptr<std::list<double>> &s,
                    enum rpn_op op, int times)
{
        double x, y, res;

        if (times == 0)
                /* apply the op for all elements */
                times = s->size() - 1;

        for (int i = 0; i < times; ++i)
        {
                if (s->size() <= 1) return;

                x = s->back();
                s->pop_back();
                y = s->back();
                s->pop_back();

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
                                std::cout << "error - division by zero"
                                        << std::endl;
                                return;
                        }

                        res = y / x;
                        break;
                }

                s->push_back(res);
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

static void cmd_double(struct rpn_cmd *cmd, std::string buf)
{
        cmd->t = DOUBLE;
        cmd->data.val = std::stod(buf);
}

static int is_valid_double(std::string buf)
{
        for (size_t i = 0, p = 0; i < buf.size(); ++i)
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


static int is_valid_int(std::string buf, int last)
{
        for (int i = 0; i < last; ++i)
                if(!isdigit(buf[i]))
                        return 0;
        return 1;
}

static int parse_token(std::string *tok, struct rpn_cmd *cmd)
{
        int tok_len = tok->size();

        /* the user pressed only enter */
        if (!tok_len)
                return -1;

        /* check for a command like drop and quit */
        for (int i = 0; i < NCMD; ++i)
        {
                if(tok->compare(rpn_commands[i].cmd) == 0)
                {
                        cmd->t = rpn_commands[i].cmd_type;
                        return 0;
                }
        }

        /* check for both an op and a signed number */
        if (tok->front() == '+' || tok->front() == '-')
        {
                if (tok_len > 1)
                {
                        /* could be a positive/negative number
                         * prefixed with sign, check the input */
                        if (is_valid_double(tok->substr(1, tok->size() - 1)))
                                cmd_double(cmd, *tok);
                        else
                                return -1;
                }
                else
                {
                        /* this is an op */
                        cmd_op(cmd, 1, tok->front());
                }
        }

        /* check for the remaining ops */
        else if (tok->front() == '*' || tok->front() == '/')
        {
                /* check for non-valid things like *3 */
                if (tok_len > 1)
                        return -1;

                /* here we are sure that this is an op */
                cmd_op(cmd, 1, tok->front());
        }

        else
        {
                int last_char_index = tok_len - 1, times;
                /* here could be a number or a multiple command like 3+, 4- */
                if (is_valid_double(tok->substr(0, last_char_index + 1)))
                        /* this is a double */
                        cmd_double(cmd, *tok);
                /* if the last char is an op and this is an int,
                 * it is a multiple command */
                else if ((tok->at(last_char_index) == '+' ||
                          tok->at(last_char_index) == '-' ||
                          tok->at(last_char_index) == '*' ||
                          tok->at(last_char_index) == '/') &&
                         /* strlen(tok) - 1 so we do not consider
                          * the final op */
                         is_valid_int(*tok, last_char_index))
                {
                        /* we have a multiple op */
                        times = std::stol(*tok);
                        cmd_op(cmd, times, tok->at(last_char_index));
                }

                else
                        return -1;

        }

        return 0;
}

static int exec_line(std::unique_ptr<std::list<double>> &s,
                     std::unique_ptr<std::string> &buf)
{
        std::string tmp;
        struct rpn_cmd cmd;
        int ret;
        size_t current, next = -1;

        memset(&cmd, 0, sizeof(rpn_cmd));
        do
        {
                current = next + 1;
                next = buf->find_first_of(" ", current);
                tmp = buf->substr(current, next - current);

                if((ret = parse_token(&tmp, &cmd)) == -1)
                        return ret;

                switch(cmd.t)
                {
                case DOUBLE:
                        s->push_back(cmd.data.val);
                        break;
                case OP:
                        if (cmd.op_times == 0)
                                exec_op(s, cmd.data.op, s->size() - 1);
                        else
                                exec_op(s, cmd.data.op, cmd.op_times);
                        break;
                case DROP:
                        s->pop_back();
                        break;
                case CLEAR:
                        s->clear();
                        break;
                }
        } while(next != std::string::npos);

        return 1;
}

static void stack_print(std::unique_ptr<std::list<double>> &s)
{
        std::for_each(s->begin(), s->end(),
                      [](double d) { std::cout << std::fixed
                                               << std::setprecision(6)
                                               << d << std::endl; });
}

int main(int argc, char *argv[])
{
        std::unique_ptr<std::list<double>> s =
                std::make_unique<std::list<double>>();
        std::unique_ptr<std::string> buf =
                std::make_unique<std::string>();
        int retval, interactive;

        if (argc == 1) interactive = 1;
        else if (argc == 2 && strcmp(argv[1], "-b") == 0) interactive = 0;
        else
                return 0;

        if (interactive) std::cout << "> ";

        /* main loop */
        while(std::getline(std::cin, *buf))
        {
                retval = exec_line(s, buf);

                switch(retval)
                {
                case -1:
                        std::cout << "error" << std::endl;
                        break;
                case 0:
                        return 0;
                case 1:
                        break;
                }

                if (interactive)
                {
                        stack_print(s);
                        std::cout << "> ";
                }
        }

        if (interactive) std::cout << '\r';
        else stack_print(s);

        return 0;
}

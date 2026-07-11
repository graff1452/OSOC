/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
word_t isa_reg_str2val(const char *s, bool *success);

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NUM, TK_NEQ, TK_AND, TK_REG, TK_DEREF,
  /* TODO: Add more token types */
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},           // minus
  {"\\*", '*'},         // multiply
  {"/", '/'},           // divide
  {"\\(", '('},         // left paren
  {"\\)", ')'},         // right paren
  {"[0-9]+", TK_NUM},   // decimal integer
  {"\\$[a-zA-Z0-9]+", TK_REG},  // register, e.g. $pc, $a0
  {"==", TK_EQ},        // equal
  {"!=", TK_NEQ},       // not equal
  {"&&", TK_AND},       // logical and
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[1024] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            // whitespace: recognized but discarded, don't record it
            break;
          default:
            if (nr_token >= 1024) {
              printf("too many tokens\n");
              return false;
            }
            tokens[nr_token].type = rules[i].token_type;
            int len = substr_len < 31 ? substr_len : 31;
            strncpy(tokens[nr_token].str, substr_start, len);
            tokens[nr_token].str[len] = '\0';
            nr_token++;
            break;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}


static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }
  int balance = 0;
  int i;
  for (i = p; i <= q; i++) {
    if (tokens[i].type == '(') balance++;
    if (tokens[i].type == ')') balance--;
    if (balance == 0 && i != q) {
      // the matching ')' for tokens[p] closed before reaching q
      return false;
    }
  }
  return balance == 0;
}

static int op_precedence(int type) {
  switch (type) {
    case TK_AND: return 0;
    case TK_EQ: case TK_NEQ: return 1;
    case '+': case '-': return 2;
    case '*': case '/': return 3;
    default: return -1;
  }
}

static int32_t eval(int p, int q, bool *success) {
  if (p > q) {
    printf("Bad expression\n");
    *success = false;
    return 0;
  }
  else if (p == q) {
    if (tokens[p].type == TK_NUM) {
      return (int32_t)atoi(tokens[p].str);
    }
    else if (tokens[p].type == TK_REG) {
      bool reg_success = true;
      word_t val = isa_reg_str2val(tokens[p].str + 1, &reg_success);
      if (!reg_success) {
        printf("Unknown register '%s'\n", tokens[p].str);
        *success = false;
        return 0;
      }
      return (int32_t)val;
    }
    else {
      printf("Expected a number or register\n");
      *success = false;
      return 0;
    }
  }
  else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  else {
    int op = -1;
    int min_prec = 1000;
    int depth = 0;
    int i;
    for (i = p; i <= q; i++) {
      if (tokens[i].type == '(') { depth++; continue; }
      if (tokens[i].type == ')') { depth--; continue; }
      if (depth > 0) continue;
      int prec = op_precedence(tokens[i].type);
      if (prec == -1) continue;
      if (prec <= min_prec) {
        min_prec = prec;
        op = i;
      }
    }
    if (op == -1) {
      printf("No operator found\n");
      *success = false;
      return 0;
    }

    int32_t val1 = eval(p, op - 1, success);
    if (!*success) return 0;
    int32_t val2 = eval(op + 1, q, success);
    if (!*success) return 0;

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        if (val2 == 0) {
          printf("Division by zero\n");
          *success = false;
          return 0;
        }
        return val1 / val2;
      case TK_EQ: return val1 == val2;
      case TK_NEQ: return val1 != val2;
      case TK_AND: return val1 && val2;
      default:
        *success = false;
        return 0;
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  *success = true;
  return (word_t)eval(0, nr_token - 1, success);
}
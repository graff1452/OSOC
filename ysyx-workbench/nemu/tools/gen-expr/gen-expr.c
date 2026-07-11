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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int buf_len = 0;

static void gen(char c) {
  if (buf_len < sizeof(buf) - 1) {
    buf[buf_len++] = c;
    buf[buf_len] = '\0';
  }
}

static void gen_str(const char *s) {
  while (*s) {
    gen(*s);
    s++;
  }
}

static void gen_num() {
  int n = rand() % 100;
  char tmp[16];
  sprintf(tmp, "%d", n);
  gen_str(tmp);
}

static void gen_rand_op() {
  char ops[] = {'+', '-', '*', '/'};
  gen(ops[rand() % 4]);
}

static int depth = 0;

static void gen_rand_expr_helper() {
  depth++;

  if (depth > 20) {
    gen_num();
  }
  else {
    switch (rand() % 3) {
      case 0: gen_num(); break;
      case 1:
        gen('(');
        gen_rand_expr_helper();
        gen(')');
        break;
      default:
        gen_rand_expr_helper();
        gen_rand_op();
        gen_rand_expr_helper();
        break;
    }
  }

  depth--;
}

static void gen_rand_expr() {
  buf_len = 0;
  buf[0] = '\0';
  depth = 0;
  gen_rand_expr_helper();
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);
    int result;
    ret = fscanf(fp, "%d", &result);
    int close_status = pclose(fp);
    if (close_status != 0) {
      // the expression program crashed (e.g. divide by zero) - skip this case
      continue;
    }
    printf("%u %s\n", result, buf);
  }
  return 0;
}

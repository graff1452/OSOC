#include <common.h>
#include <stdio.h>

word_t expr(char *e, bool *success);
void init_regex();
uint64_t g_nr_guest_inst = 0;
void assert_fail_msg() {}

int main() {
  init_regex();
  FILE *fp = fopen("/tmp/single_test", "r");
  if (fp == NULL) {
    printf("Could not open input file\n");
    return 1;
  }

  char line[65536];
  int total = 0, passed = 0;

  while (fgets(line, sizeof(line), fp) != NULL) {
    word_t expected;
    char expr_str[65536];
    if (sscanf(line, "%u %[^\n]", &expected, expr_str) != 2) {
      continue;
    }

    total++;
    bool success = true;
    word_t result = expr(expr_str, &success);

    if (!success) {
      printf("FAIL (eval error): %s\n", expr_str);
    }
    else if (result != expected) {
      printf("FAIL: expected %u, got %u: %s\n", expected, result, expr_str);
    }
    else {
      passed++;
    }
  }

  fclose(fp);
  printf("Passed %d / %d\n", passed, total);
  return 0;
}
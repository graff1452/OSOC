#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

typedef struct { void *start, *end; } Area;
Area heap = { (void *)0x87000000, (void *)0x87f00000 };

int main(int argc, char *argv[], char *envp[]);
extern char **environ;
void call_main(uintptr_t *args) {
  char *empty[] =  {NULL };
  environ = empty;
  exit(main(0, empty, empty));
  assert(0);
}
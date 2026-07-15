#include <common.h>
#include <elf.h>
#include <stdio.h>

#define MAX_FUNCS 512

typedef struct {
  paddr_t addr;
  uint32_t size;
  char name[64];
} FuncSym;

static FuncSym funcs[MAX_FUNCS];
static int nr_funcs = 0;
static int depth = 0;

void init_ftrace(const char *elf_file) {
  if (elf_file == NULL) {
    Log("No ELF file given, ftrace will show raw addresses only.");
    return;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp, "Can not open '%s'", elf_file);

  int nread;

  Elf32_Ehdr ehdr;
  nread = fread(&ehdr, sizeof(ehdr), 1, fp);
  assert(nread == 1);

  Elf32_Shdr *shdrs = malloc(sizeof(Elf32_Shdr) * ehdr.e_shnum);
  fseek(fp, ehdr.e_shoff, SEEK_SET);
  nread = fread(shdrs, sizeof(Elf32_Shdr), ehdr.e_shnum, fp);
  assert(nread == ehdr.e_shnum);

  Elf32_Shdr *symtab_hdr = NULL, *strtab_hdr = NULL;
  // the section header string table tells us each section's name
  Elf32_Shdr *shstrtab_hdr = &shdrs[ehdr.e_shstrndx];
  char *shstrtab = malloc(shstrtab_hdr->sh_size);
  fseek(fp, shstrtab_hdr->sh_offset, SEEK_SET);
  nread = fread(shstrtab, shstrtab_hdr->sh_size, 1, fp);
  assert(nread == 1);

  int i;
  for (i = 0; i < ehdr.e_shnum; i++) {
    char *name = shstrtab + shdrs[i].sh_name;
    if (strcmp(name, ".symtab") == 0) symtab_hdr = &shdrs[i];
    if (strcmp(name, ".strtab") == 0) strtab_hdr = &shdrs[i];
  }
  Assert(symtab_hdr && strtab_hdr, "ELF file has no symbol table (was it stripped?)");

  char *strtab = malloc(strtab_hdr->sh_size);
  fseek(fp, strtab_hdr->sh_offset, SEEK_SET);
  nread = fread(strtab, strtab_hdr->sh_size, 1, fp);
  assert(nread == 1);

  int nr_syms = symtab_hdr->sh_size / sizeof(Elf32_Sym);
  Elf32_Sym *syms = malloc(symtab_hdr->sh_size);
  fseek(fp, symtab_hdr->sh_offset, SEEK_SET);
  nread = fread(syms, symtab_hdr->sh_size, 1, fp);
  assert(nread == 1);

  for (i = 0; i < nr_syms && nr_funcs < MAX_FUNCS; i++) {
    if (ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_size > 0) {
      funcs[nr_funcs].addr = syms[i].st_value;
      funcs[nr_funcs].size = syms[i].st_size;
      snprintf(funcs[nr_funcs].name, sizeof(funcs[nr_funcs].name), "%s", strtab + syms[i].st_name);
      nr_funcs++;
    }
  }

  free(shstrtab); free(strtab); free(syms); free(shdrs);
  fclose(fp);
  Log("ftrace: loaded %d function symbols from %s", nr_funcs, elf_file);
}

static const char *addr_to_func(paddr_t addr) {
  int i;
  for (i = 0; i < nr_funcs; i++) {
    if (addr >= funcs[i].addr && addr < funcs[i].addr + funcs[i].size) {
      return funcs[i].name;
    }
  }
  return "???";
}

void ftrace_call(paddr_t pc, paddr_t target) {
  if (nr_funcs == 0) return; // no ELF loaded, nothing to report
  int i;
  printf("0x%08x:", pc);
  for (i = 0; i < depth; i++) printf("  ");
  printf("call [%s@0x%08x]\n", addr_to_func(target), target);
  depth++;
}

void ftrace_ret(paddr_t pc) {
  if (nr_funcs == 0) return;
  if (depth > 0) depth--;
  int i;
  printf("0x%08x:", pc);
  for (i = 0; i < depth; i++) printf("  ");
  printf("ret  [%s]\n", addr_to_func(pc));
}

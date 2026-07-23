#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

size_t fs_filesz(const char *filename);
size_t fs_read(const char *filename, void *buf, size_t offset, size_t len);

uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;
  fs_read(filename, &ehdr, 0, sizeof(Elf_Ehdr));

  assert(ehdr.e_ident[EI_MAG0] == ELFMAG0);
  assert(ehdr.e_ident[EI_MAG1] == ELFMAG1);
  assert(ehdr.e_ident[EI_MAG2] == ELFMAG2);
  assert(ehdr.e_ident[EI_MAG3] == ELFMAG3);

  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf_Phdr phdr;
    fs_read(filename, &phdr, ehdr.e_phoff + i * sizeof(Elf_Phdr), sizeof(Elf_Phdr));

    if (phdr.p_type == PT_LOAD) {
      fs_read(filename, (void *)phdr.p_vaddr, phdr.p_offset, phdr.p_filesz);
      if (phdr.p_memsz > phdr.p_filesz) {
        memset((void *)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
      }
    }
  }

  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}
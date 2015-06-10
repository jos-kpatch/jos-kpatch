#include <inc/lib.h>
#include <inc/elf.h>

int
load_code(char *filename, void *target)
{
	int r, fd, i;
	unsigned char elf_buf[512];
	struct Elf *elf;
	struct Proghdr *ph, *eph;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		printf("cannot open %s\n", filename);
		exit();
	}

	elf = (struct Elf*) elf_buf;
	if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
	    || elf->e_magic != ELF_MAGIC) {
		close(fd);
		cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
		exit();
	}

	// we only care the first ph
	ph = (struct Proghdr *) ((uint8_t *) elf + elf->e_phoff);

	for (i = 0; i < ph->p_memsz; i += PGSIZE) {
		if (i >= ph->p_filesz) {
			// allocate a blank page
			if ((r = sys_page_alloc(0, target + i, PTE_P|PTE_U|PTE_W)) < 0) {
				printf("sys_page_alloc: %e\n", r);
				exit();
			}
			continue;
		}

		if ((r = sys_page_alloc(0, target + i, PTE_P|PTE_U|PTE_W)) < 0) {
			printf("sys_page_alloc: %e\n", r);
			exit();
		}
		if ((r = seek(fd, ph->p_offset + i)) < 0) {
			printf("seek: %e\n", r);
			exit();
		}
		if ((r = readn(fd, target + i, MIN(PGSIZE, ph->p_filesz-i))) < 0) {
			printf("read: %e\n", r);
			exit();
		}

	}

	return sys_load_code((void *) ph->p_va, target, ph->p_memsz, (void *) elf->e_entry);
}

void
umain(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <module.ko>\n", argv[0]);
		exit();
	}

	load_code(argv[1], UTEMP);
}


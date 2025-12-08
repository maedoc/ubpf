#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <stddef.h>
#include <sys/types.h>

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

static inline int mprotect(void *addr, size_t len, int prot) { return 0; }
static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) { return MAP_FAILED; }
static inline int munmap(void *addr, size_t length) { return 0; }

#endif

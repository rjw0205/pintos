#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/filesys.h"
#include "lib/kernel/list.h"

struct file_descriptor
{
  int fd;
  struct file * file;
  struct list_elem elem;
};


void syscall_init (void);

#endif /* userprog/syscall.h */

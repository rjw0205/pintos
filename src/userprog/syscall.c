#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

struct lock syscall_lock;

void
syscall_init (void) 
{
  lock_init(&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

///////////// helper functions to access user memory
static int
get_user(const uint8_t *uaddr){
  if(is_user_vaddr(uaddr)){
    int result;
    asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
    return result;
  }
  return -1;
}

static int
get_user_many(const uint8_t *start, int how_many, void *destination){
  int cnt;

  for(cnt=0; cnt<how_many; cnt++){
    int result = get_user(start+cnt);
    if(result == -1){
      our_exit(-1);
    }
    *((char *)destination + cnt) = result;
  }
  return 0;
}

static bool
put_user (uint8_t *udst, uint8_t byte)
{
  if(is_user_vaddr(udst)){
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
         : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
  }
  return -1;
}

///////////////////////////////////////////////////

void
our_halt(){
  shutdown_power_off();
}

void
our_exit(int status){

  printf("%s: exit(%d)\n", thread_current()->name, status);
//  lock_acquire(&syscall_lock);
  thread_current()->exit_status = status;
//  lock_release(&syscall_lock);
  thread_exit();

}

int
our_wait(tid_t tid){
  //lock_acquire(&syscall_lock);
  int x = process_wait(tid);
  //lock_release(&syscall_lock);
  return x;
}

tid_t
our_exec(const char *file){
  //printf("fff\n");
  //printf("%d!!\n", strlen(file));
  tid_t x = process_execute(file);
  return x;
}

int
our_write(int fd, const void *buffer, unsigned size){
  unsigned cnt = 0;
  //printf("%x\n", buffer);
  // while( is_user_vaddr((char *)buffer+cnt) && *((char *)buffer+cnt) != '\0' && cnt<size){
  //   put_user(fd, *((char *)buffer+cnt));
  //   cnt++;
  // }
  if (fd ==1){
    putbuf(buffer,size);
    return size;
  }
  else
    return size;
}

bool
our_create(const char *file, unsigned initial_size){
  //lock_acquire(&syscall_lock);
  bool success = filesys_create(file, initial_size);
  //lock_release(&syscall_lock);
  return success;
}

bool
our_remove(const char *file){
  bool success = filesys_remove(file);
  return success;
}

static void
syscall_handler (struct intr_frame *f) 
{
  // printf("%x\n",f->esp);
  // printf ("system call!\n");
  // thread_exit ();

  int syscallnumber;
//  if (get_user_many(f->esp, 4, &syscallnumber) == -1){
//    our_exit(-1);
//    return;
//  }
  get_user_many(f->esp, 4, &syscallnumber);

  switch(syscallnumber){
    case SYS_HALT: // 0
    {                   /* Halt the operating system. */
      our_halt();
      break;
    }
    case SYS_EXIT: // 1
    {                   /* Terminate this process. */
      int status;
      get_user_many(f->esp+4, 4, &status);             
      our_exit(status);
      break;
    }
    case SYS_EXEC: // 2
    {                   /* Start another process. */
      char *file;
      get_user_many(f->esp+4, 4, &file);

      if(get_user(file) == -1){
        our_exit(-1);
      }
      else{
        f->eax = (uint32_t)our_exec(file);
      }

      break;
    }
    case SYS_WRITE: // 9
    {
      int fd;
      void *buffer;
      unsigned size;
      get_user_many(f->esp+4, 4, &fd);
      get_user_many(f->esp+8, 4, &buffer);
      get_user_many(f->esp+12, 4, &size);

      if(get_user(buffer) == -1){
        our_exit(-1);
      }
      else{
        f->eax = (uint32_t)our_write(fd, buffer, size);
      }
      break;
    }
    case SYS_WAIT: // 3
    {
      tid_t tid;
      get_user_many(f->esp+4, 4, &tid);
      f->eax = (uint32_t)our_wait(tid);
      break;
    }
    case SYS_CREATE: // 4
    {
      char *file;
      unsigned initial_size;
      get_user_many(f->esp+4, 4, &file);
      get_user_many(f->esp+8, 4, &initial_size);
      if(get_user(file) == -1){
        our_exit(-1);
      }
      else {
        f->eax = our_create(file, initial_size);
      }
      break;
    }
    case SYS_REMOVE: // 5
    {
      char *file;
      get_user_many(f->esp+4, 4, &file);
      if(get_user(file) == -1){
        our_exit(-1);
      }
      else {
        f->eax = our_remove(file);
      }
      break;
    }
    default:
      break;
    //SYS_CREATE,                 /* Create a file. */
    //SYS_REMOVE,                 /* Delete a file. */
    //SYS_OPEN,                   /* Open a file. */
    //SYS_FILESIZE,               /* Obtain a file's size. */
    //SYS_READ,                   /* Read from a file. */
    //SYS_WRITE,                  /* Write to a file. */
    //SYS_SEEK,                   /* Change position in a file. */
    //SYS_TELL,                   /* Report current position in a file. */
    //SYS_CLOSE, 
  }
}

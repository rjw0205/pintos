#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

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
      return -1;
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
  thread_exit();
}

tid_t
our_exec(const char *file){
  return process_execute(file);
}

void
our_write(int fd, const void *buffer, unsigned size){
  unsigned cnt = 0;
  //printf("%x\n", buffer);
  // while( is_user_vaddr((char *)buffer+cnt) && *((char *)buffer+cnt) != '\0' && cnt<size){
  //   put_user(fd, *((char *)buffer+cnt));
  //   cnt++;
  // }
  putbuf(buffer, size);
}




void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf("%x\n",f->esp);
  // printf ("system call!\n");
  // thread_exit ();

  int syscallnumber;
  ASSERT(get_user_many(f->esp, 4, &syscallnumber) != -1);
  //printf("%d\n", syscallnumber);
  switch(syscallnumber){
    case SYS_HALT:
    {                   /* Halt the operating system. */
      our_halt();
      break;
    }
    case SYS_EXIT:
    {                   /* Terminate this process. */
      int status;
      get_user_many(f->esp+4, 4, &status);             
      our_exit(status);
      break;
    }
    case SYS_EXEC:
    {                   /* Start another process. */
      char *file;
      get_user_many(f->esp+4, strlen(f->esp+4), file);
      our_exec(file);
      break;
    }
    case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      get_user_many(f->esp+4, 4, &fd);
      get_user_many(f->esp+8, 4, &buffer);
      get_user_many(f->esp+12, 4, &size);
      our_write(fd, buffer, size);
      break;
    }
    default:
      break;
    //SYS_WAIT,                   /* Wait for a child process to die. */
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















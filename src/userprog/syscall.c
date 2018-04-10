#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

static struct lock syscall_lock;
static struct lock fd_lock;

void
syscall_init (void) 
{
  lock_init(&syscall_lock);
  lock_init(&fd_lock);
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

int
allocate_fd (void) 
{
  static int next_fd = 2;
  int fd;
  lock_acquire (&fd_lock);
  fd = next_fd++;
  lock_release (&fd_lock);
  return fd;
}

struct file *
find_file_using_fd(int input_fd) {
  struct list_elem *e;
  struct thread * cur = thread_current();
  for (e = list_begin (&cur->open_file_list); e != list_end (&cur->open_file_list);
       e = list_next (e))
    {
      struct file_descriptor * f = list_entry (e, struct file_descriptor, elem);
      if(f->fd == input_fd){
        return f->file;
      }
    }
    return NULL;
}

struct file_descriptor *
find_descriptor_using_fd(int input_fd) {
  struct list_elem *e;
  struct thread * cur = thread_current();
  for (e = list_begin (&cur->open_file_list); e != list_end (&cur->open_file_list);
       e = list_next (e))
    {
      struct file_descriptor * f = list_entry (e, struct file_descriptor, elem);
      if(f->fd == input_fd){
        return f;
      }
    }
    return NULL;
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
  lock_acquire(&syscall_lock);
  tid_t x = process_execute(file);
  lock_release(&syscall_lock);
  return x;
}

bool
our_create(const char *file, unsigned initial_size){
  lock_acquire(&syscall_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&syscall_lock);
  return success;
}

bool
our_remove(const char *file){
  lock_acquire(&syscall_lock);
  bool success = filesys_remove(file);
  lock_release(&syscall_lock);
  return success;
}

int
our_open(const char * file){
  struct file * file_opened;
  int fd;

  struct file_descriptor * file_desc;
  lock_acquire(&syscall_lock);
  file_opened = filesys_open(file);
  lock_release(&syscall_lock);
  if (file_opened == NULL)
    return -1;

  file_desc = palloc_get_page(0);
  if(file_desc == NULL)
    our_exit(-1);
  file_desc->fd = allocate_fd();
  file_desc->file = file_opened;
  list_push_front(&thread_current()->open_file_list, &file_desc->elem);
  return file_desc->fd;
}

int
our_filesize(int fd){
  struct file * file_opened;
  file_opened = find_file_using_fd(fd);
  if (file_opened == NULL)
    return -1;
  else
  {
    int len;
    lock_acquire(&syscall_lock);
    len = file_length(file_opened);
    lock_release(&syscall_lock);
    return len;
  }
}

int
our_read(int fd, const void *buffer, unsigned size){
  struct file * file_opened;
  file_opened = find_file_using_fd(fd);
  if (file_opened == NULL)
    return -1;
  int len;
  lock_acquire(&syscall_lock);
  len = file_read(file_opened, buffer, size);
  lock_release(&syscall_lock);
  return len;
}

int
our_write(int fd, const void *buffer, unsigned size){
  if (fd ==1){
    putbuf(buffer,size);
    return size;
  }
  else{
    struct file * file_opened;
    file_opened = find_file_using_fd(fd);
    if (file_opened == NULL)
      return -1;
    int len;
    lock_acquire(&syscall_lock);
    len = file_write(file_opened, buffer, size);
    lock_release(&syscall_lock);
    return len;
  }
}

void
our_seek(int fd, unsigned position)
{
  struct file * file_opened;
  file_opened = find_file_using_fd(fd);
  if (file_opened == NULL)
    our_exit(-1);
  else
  {
    lock_acquire(&syscall_lock);
    file_seek(file_opened,position);
    lock_release(&syscall_lock);
  }
}
unsigned
our_tell(int fd)
{
  struct file * file_opened;
  file_opened = find_file_using_fd(fd);
  if (file_opened == NULL)
    return -1;
  else
  {
    int len;
    lock_acquire(&syscall_lock);
    len = file_tell(file_opened);
    lock_release(&syscall_lock);
    return len;
  }
}

void
our_close(int fd)
{
  struct file * file_opened;
  struct file_descriptor * descriptor = find_descriptor_using_fd(fd);
  file_opened = find_file_using_fd(fd);
  if (file_opened == NULL)
    our_exit(-1);
  else
  {
    lock_acquire(&syscall_lock);
    file_close(file_opened);
    lock_release(&syscall_lock);
    list_remove(&descriptor->elem);
    palloc_free_page(descriptor);
  }
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscallnumber;
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
      const char *file;
      get_user_many(f->esp+4, 4, &file);
      if(get_user(file) == -1){
        our_exit(-1);
      }
      else {
        f->eax = our_remove(file);
      }
      break;
    }
    case SYS_OPEN: // 6
    {
      const char *file;
      get_user_many(f->esp+4, 4, &file);
      if(get_user(file) == -1){
        our_exit(-1);
      }
      else {
        f->eax = our_open(file);
      }
      break;
    }
    case SYS_FILESIZE: // 7
    {
      int fd;
      get_user_many(f->esp+4, 4, &fd);
      f->eax = our_filesize(fd);
      break;
    }
    case SYS_READ:
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
      else if(buffer == NULL)
      {
        our_exit(-1);
      }
      else{
        f->eax = (uint32_t)our_read(fd, buffer, size);
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
        f->eax = our_write(fd, buffer, size);
      }
      break;
    }
    case SYS_SEEK:
    {
      int fd;
      unsigned position;
      get_user_many(f->esp+4, 4, &fd);
      get_user_many(f->esp+8, 4, &position);
      our_seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      int fd;
      get_user_many(f->esp+4, 4, &fd);
      f->eax = our_tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
      int fd;
      get_user_many(f->esp+4, 4, &fd);
      our_close(fd);
      break;
    }
    default:
      break;
  }
}

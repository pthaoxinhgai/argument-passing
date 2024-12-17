#include "process.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
//#include "vm/vm.h"

static struct semaphore load_sema; // semaphore đồng bộ hóa cho quá trình load
static struct semaphore start_sema; // semaphore đồng bộ hóa cho quá trình start

/* Khởi tạo semaphores */
void process_init(void) {
  sema_init(&load_sema, 0);
  sema_init(&start_sema, 0);
}

/* Khởi tạo process */
bool process_execute(const char *file_name) {
  char *fn_copy;
  bool success = false;

  /* Kiểm tra bộ nhớ đã cấp phát thành công */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL) {
    printf("Error: Failed to allocate memory for filename.\n");
    return false;
  }
  
  strlcpy(fn_copy, file_name, PGSIZE);
  
  /* Tạo một thread mới để bắt đầu process */
  if (thread_create(file_name, PRI_DEFAULT, start_process, fn_copy)) {
    sema_down(&start_sema);  // Chờ thread hoàn tất
    success = true;
  }

  if (!success) {
    palloc_free_page(fn_copy);
  }

  return success;
}

/* Quá trình khởi tạo start */
static void start_process(void *file_name_) {
  char *file_name = file_name_;
  bool success = false;

  /* Đảm bảo quá trình con hoàn tất trước khi tiếp tục */
  sema_up(&start_sema);  // Đồng bộ hóa để main thread có thể tiếp tục

  /* In hex dump để debug */
  hex_dump(0, file_name, PGSIZE, true);

  /* Khởi tạo các biến cần thiết cho việc load process */
  success = load(file_name);

  if (!success) {
    printf("Error: Failed to load executable %s\n", file_name);
    thread_exit();  // Kết thúc thread nếu không thể tải chương trình
  }

  /* Chờ signal từ main thread để tiếp tục */
  sema_up(&load_sema);  // Đồng bộ hóa với load process

  thread_exit();  // Kết thúc thread khi hoàn tất
}

/* Load một executable */
static bool load(const char *file_name) {
  struct file *file = NULL;
  struct elf_header ehdr;
  bool success = false;
  int i;

  /* Mở file executable */
  file = filesys_open(file_name);
  if (file == NULL) {
    printf("Error: Failed to open file %s\n", file_name);
    return false;
  }

  /* Đọc ELF header */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || !is_elf(&ehdr)) {
    printf("Error: %s is not a valid ELF file.\n", file_name);
    file_close(file);
    return false;
  }

  /* In hex dump cho các segment */
  hex_dump(0, &ehdr, sizeof(ehdr), true);

  /* Kiểm tra các segment và load chúng */
  for (i = 0; i < ehdr.e_phnum; i++) {
    struct program_header phdr;
    
    /* Đọc header của segment */
    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) {
      printf("Error: Failed to read program header.\n");
      file_close(file);
      return false;
    }
    
    /* In hex dump cho từng segment */
    hex_dump(0, &phdr, sizeof(phdr), true);

    /* Xử lý segment */
    if (!load_segment(file, &phdr)) {
      printf("Error: Failed to load segment.\n");
      file_close(file);
      return false;
    }
  }

  /* Khởi tạo ngăn xếp và thực thi chương trình */
  success = setup_stack(file_name);

  /* In hex dump cho stack setup */
  hex_dump(0, (void *)USER_STACK, PGSIZE, true);

  /* Đảm bảo file được đóng khi không cần thiết nữa */
  file_close(file);
  return success;
}

/* Hàm load segment */
static bool load_segment(struct file *file, struct program_header *phdr) {
  /* Kiểm tra lỗi nếu segment không hợp lệ */
  if (phdr->p_type != PT_LOAD) {
    printf("Error: Invalid segment type.\n");
    return false;
  }
  
  /* Tải segment vào bộ nhớ */
  if (!valid_segment(phdr)) {
    printf("Error: Invalid segment.\n");
    return false;
  }

  /* Cài đặt trang cho segment */
  if (!install_page(phdr->p_vaddr, file, phdr->p_offset, phdr->p_filesz)) {
    printf("Error: Failed to install page.\n");
    return false;
  }

  return true;
}

/* Kiểm tra tính hợp lệ của segment */
static bool valid_segment(struct program_header *phdr) {
  if (phdr->p_vaddr < USER_VADDR_START || phdr->p_vaddr >= USER_VADDR_LIMIT) {
    printf("Error: Segment address out of bounds.\n");
    return false;
  }

  /* Kiểm tra quyền truy cập và loại segment */
  if (phdr->p_flags & PF_X && !(phdr->p_flags & PF_R)) {
    printf("Error: Executable segment without read permission.\n");
    return false;
  }

  return true;
}

/* Thiết lập stack */
static bool setup_stack(const char *file_name) {
  uint8_t *kpage;
  bool success = false;

  /* Cấp phát bộ nhớ cho stack */
  kpage = palloc_get_page(PAL_USER);
  if (kpage != NULL) {
    success = install_page((void *) USER_STACK, kpage, true);
  } else {
    printf("Error: Failed to allocate memory for stack.\n");
  }

  return success;
}

/* Cài đặt trang vào bộ nhớ */
static bool install_page(void *upage, void *kpage, bool writable) {
  struct thread *t = thread_current();
  
  /* Kiểm tra xem địa chỉ đã được cài đặt chưa */
  if (pagedir_get_page(t->pagedir, upage) != NULL) {
    return false;
  }

  /* Cài đặt trang */
  return pagedir_set_page(t->pagedir, upage, kpage, writable);
}

/* Xử lý exit */
void process_exit(void) {
  /* Giải phóng tài nguyên, bao gồm semaphore */
  sema_up(&load_sema);  // Giải phóng semaphore load_sema
  sema_up(&start_sema);  // Giải phóng semaphore start_sema
}


#include "memory.h"
#include "bitmap.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "print.h"
#include "stdint.h"
#include "string.h"
#include "sync.h"

#define PG_SIZE 4096

/***************  bitmap address ********************
 * 0xc009f000: kernel main thread stack top
 * 0xc009e000: kernel main thread pcb.
 * a page bitmap represents 128M memory
 * bitmap located at 0xc009a000
 * totally 4 pages bitmap .indicating 512M */
#define MEM_BITMAP_BASE 0xc009a000
/*************************************/

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 0xc0000000: VA 3G
   0x100000 strides 1MB, making VA continuous */
#define K_HEAP_START 0xc0100000

/* memory pool */
typedef struct _POOL {
  struct bitmap pool_bitmap; // Bitmap. Control PYHCISAL MEMORY
  uint32_t phy_addr_start;   // PYHSICAL MEMORY START
  uint32_t pool_size;        // Byte size
  LOCK lock;
} POOL, *PPOOL;

typedef struct _ARENA {
  PMEM_BLOCK_DESC desc;
  uint32_t cnt;
  bool large; // when large==true, cnt means page numbers
              // otherwise, cnt means numbers of spare mem_block
} ARENA, *PARENA;

MEM_BLOCK_DESC k_block_descs[DESC_CNT];
POOL kernel_pool, user_pool;
VISUAL_ADDRESS kernel_vaddr; // malloc for kernel

// malloc pg_cnt size pages in PF)
// return VA if success
// return NULL if fail
static void *vaddr_get(POOL_FLAGS pf, uint32_t pg_cnt) {
  int vaddr_start = 0, bit_idx_start = -1;
  uint32_t cnt = 0;
  if (pf == PF_KERNEL) {
    bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
    if (bit_idx_start == -1) {
      return NULL;
    }
    while (cnt < pg_cnt) {
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
      // cnt++;
    }
    vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
  } else {
    // user memory pool
    PTASK_STRUCT cur = get_running_thread();
    bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
    if (bit_idx_start == -1) {
      return NULL;
    }

    while (cnt < pg_cnt) {
      bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
    }
    vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

    //(0xc0000000 - PG_SIZE)has been malloc as USER STACK in start_process
    ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
  }
  return (void *)vaddr_start;
}

// get PTE's va by input va
uint32_t *pte_ptr(uint32_t vaddr) {
  // access physical addr by PDE 1023(WHICH point to PDE base physical addr)
  // offset by PDE to get PTE base addr(here cheat CPU with PTE bits)
  // offset by PTE to get PTE addr (here cheat CPU with OFFSET, so we should mul
  // single PTE size)
  /*return (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +
                      PTE_IDX(vaddr) * 4);*/
  uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +
                               PTE_IDX(vaddr) * 4);
  return pte;
}

// get PDE's va by input va
uint32_t *pde_ptr(uint32_t vaddr) {
  // 0xFFFFF: get pa of PageTable
  return (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
};

// malloc one PhysicalPage in m_pool
// return page physical address if success
// return NULL if failed
static void *palloc(POOL *m_pool) {
  // Ensure Atomic operation
  int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); // find page
  if (bit_idx == -1) {
    return NULL;
  }
  bitmap_set(&m_pool->pool_bitmap, bit_idx,
             1); // set bit_idx 1, indicating used
  uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
  return (void *)page_phyaddr;
}

// add PTE mapping from _va to _pa
static void page_table_add(void *_vaddr, void *_page_phyaddr) {
  uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t *pde = pde_ptr(vaddr);
  uint32_t *pte = pte_ptr(vaddr);

  if (*pde & 0x00000001) {        // PDE&PDE 0bit P,indicating present
    ASSERT(!(*pte & 0x00000001)); // this pte shouldn't present. otherwise
                                  // PANIC("pte repeat!");
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
  } else { // Page Directory doesn't exist. Here create PDE first, then create
           // PTE
    // malloc from kernel space
    uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

    *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

    /* memset physical memory 0
     * pde:(int)pte & 0xfffff000)
     */
    memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE);

    ASSERT(!(*pte & 0x00000001));
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
  }
}

// malloc pg_cnt page
void *malloc_page(POOL_FLAGS pf, uint32_t pg_cnt) {
  ASSERT(pg_cnt > 0 && pg_cnt < 3840);
  /**********************   malloc_page   **********************
     1.vaddr_get: get virtual address in virtuall memory pool
     2.palloc: get physical address in physical memory pool
     3.page_table_add mapping from va to pa in page_table
  ***************************************************************/
  void *vaddr_start = vaddr_get(pf, pg_cnt);
  if (vaddr_start == NULL) {
    return NULL;
  }

  uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
  PPOOL mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

  // va is continuous, but physical isn't continuous. So here mapping one by one
  while (cnt-- > 0) {
    void *page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) { // IF FAIL, SHOULD BACK ALL MEMORY!!!
      return NULL;
    }
    page_table_add((void *)vaddr, page_phyaddr); // mapping in page_table
    vaddr += PG_SIZE;                            // next page
  }
  return vaddr_start;
}

// malloc pg_cnt page from kernel memory pool
void *get_kernel_pages(uint32_t pg_cnt) {
  lock_acquire(&kernel_pool.lock);
  void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
  if (vaddr != NULL) {
    memset(vaddr, 0, pg_cnt * PG_SIZE);
  }
  lock_release(&kernel_pool.lock);
  return vaddr;
}
// malloc 4K memory in user mem space,return VA
void *get_user_pages(uint32_t pg_cnt) {
  lock_acquire(&user_pool.lock);
  void *vaddr = malloc_page(PF_USER, pg_cnt);
  memset(vaddr, 0, pg_cnt * PG_SIZE);
  lock_release(&user_pool.lock);
  return vaddr;
}

// mapping pf pool and va. support 1 Page size
void *get_a_page(POOL_FLAGS pf, uint32_t vaddr) {
  PPOOL mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);

  // set virtual address bitmap 1
  PTASK_STRUCT cur = get_running_thread();
  int32_t bit_idx = -1;

  // if user process, set process's own va bitmap
  if (cur->pgdir != NULL && pf == PF_USER) {
    bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);

  } else if (cur->pgdir == NULL && pf == PF_KERNEL) {

    // if kernel thread,set kernel_vaddr
    bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
  } else {
    PANIC("get_a_page:not allow kernel alloc userspace or user alloc "
          "kernel space by get_a_page");
  }

  void *page_phyaddr = palloc(mem_pool);
  if (page_phyaddr == NULL) {
    return NULL;
  }
  page_table_add((void *)vaddr, page_phyaddr);
  lock_release(&mem_pool->lock);
  return (void *)vaddr;
}

// get physical of virtual address
uint32_t addr_v2p(uint32_t vaddr) {
  uint32_t *pte = pte_ptr(vaddr);
  //(*pte) sub Attribute + vaddr low 12 bit
  return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// init memory pool
static void mem_pool_init(uint32_t all_mem) {
  put_str("   mem_pool_init start\n");

  // Page Size: PDE+ 0th & 768th PTE (same PageTable) + 769th-1022thPTE(254 num)
  // = 256 number Pages
  uint32_t page_table_size = PG_SIZE * 256;

  uint32_t used_mem = page_table_size + 0x100000; // 0x100000 low 1M memory
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem / PG_SIZE; // count Page number only
  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;

  // To simplify,here divide 8 directly.
  // This will lost some memory
  // bitmap present less memory than physical memory.
  uint32_t kbm_length = (kernel_free_pages / 8); // Kernel BitMap length. Byte
  uint32_t ubm_length = (user_free_pages / 8);   // User BitMap length.

  uint32_t kp_start = used_mem; // Kernel Pool start
  uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE; // User Pool start

  kernel_pool.phy_addr_start = kp_start;
  user_pool.phy_addr_start = up_start;

  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  user_pool.pool_size = user_free_pages * PG_SIZE;

  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

  // Global bitmap array
  // highest addr: 0xc009f000  i.e. main thread's stack bottom
  // 32M mem use 2K bitmap, so bitmap located at MEM_BITMAP_BASE(0xc009a000)
  kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;

  // user mem space forwarding kernel space
  user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length);

  put_str("      kernel_pool_bitmap_start:");
  put_int((int)kernel_pool.pool_bitmap.bits);
  put_str(" kernel_pool_phy_addr_start:");
  put_int(kernel_pool.phy_addr_start);
  put_str("\n");
  put_str("      user_pool_bitmap_start:");
  put_int((int)user_pool.pool_bitmap.bits);
  put_str(" user_pool_phy_addr_start:");
  put_int(user_pool.phy_addr_start);
  put_str("\n");

  // set bitmap 0
  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);

  lock_init(&kernel_pool.lock);
  lock_init(&user_pool.lock);

  // init kernel va bitmap
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

  // bitmap record array located out of kernel and user space
  kernel_vaddr.vaddr_bitmap.bits =
      (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);

  kernel_vaddr.vaddr_start = K_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);
  put_str("   mem_pool_init done\n");
}

void block_desc_init(PMEM_BLOCK_DESC desc_array) {
  uint16_t desc_idx, block_size = 16;

  // init each mem_block_desc
  for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
    desc_array[desc_idx].block_size = block_size;

    // init numbers of memory block in arena
    desc_array[desc_idx].blocks_per_arena =
        (PG_SIZE - sizeof(ARENA)) / block_size;

    list_init(&desc_array[desc_idx].free_list);

    block_size *= 2; // update next block size
  }
}

// return the address of the IDX_th memory block in arena.
static PMEM_BLOCK arena2block(PARENA a, uint32_t idx) {
  return (PMEM_BLOCK)((uint32_t)a + sizeof(ARENA) + idx * a->desc->block_size);
}

// return the arena address of the memory block B.
static PARENA block2arena(PMEM_BLOCK b) {
  return (PARENA)((uint32_t)b & 0xfffff000);
}

// malloc size byte memory in heap
void *sys_malloc(uint32_t size) {
  POOL_FLAGS PF;
  PPOOL mem_pool;
  uint32_t pool_size;
  PMEM_BLOCK_DESC descs;
  PTASK_STRUCT cur_thread = running_thread();

  // judge which memory pool by thread_type
  if (cur_thread->pgdir == NULL) { // kernel thread
    PF = PF_KERNEL;
    pool_size = kernel_pool.pool_size;
    mem_pool = &kernel_pool;
    descs = k_block_descs;
  } else { // user thread.
    PF = PF_USER;
    pool_size = user_pool.pool_size;
    mem_pool = &user_pool;
    descs = cur_thread->u_block_desc;
  }

  // judge memory pool capacity range
  if (!(size > 0 && size < pool_size)) {
    return NULL;
  }
  PARENA a;
  PMEM_BLOCK b;
  lock_acquire(&mem_pool->lock);

  // malloc page if size>1024
  if (size > 1024) {
    uint32_t page_cnt =
        DIV_ROUND_UP(size + sizeof(ARENA), PG_SIZE); // floor page number

    a = malloc_page(PF, page_cnt);

    if (a != NULL) {
      memset(a, 0, page_cnt * PG_SIZE); // zero set memory

      // malloc large page: set large=true, cnt=page numbers
      a->desc = NULL;
      a->cnt = page_cnt;
      a->large = true;
      lock_release(&mem_pool->lock);
      return (void *)(a + 1); // skip sizeof arena
    } else {
      lock_release(&mem_pool->lock);
      return NULL;
    }
  } else { // find in mem_block_desc if size < 1024
    uint8_t desc_idx;

    // find suitable block in mem desc from smallest to largest block
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
      if (size <= descs[desc_idx].block_size) {
        break;
      }
    }

    /* if there's no suitable mem_block in mem_block_desc free_list
     * create new arena to provide enough mem_block */
    if (list_empty(&descs[desc_idx].free_list)) {
      a = malloc_page(PF, 1); // arena size: 1 page
      if (a == NULL) {
        lock_release(&mem_pool->lock);
        return NULL;
      }
      memset(a, 0, PG_SIZE);

      /* small block memory
       * desc: memory desc
       * cnt: arena free block numbers;
       * large: false
       * */
      a->desc = &descs[desc_idx];
      a->large = false;
      a->cnt = descs[desc_idx].blocks_per_arena;
      uint32_t block_idx;

      // disable interrupt
      INTR_STATUS old_status = intr_disable();

      // split arena to block, and add to free_list
      for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena;
           block_idx++) {
        b = arena2block(a, block_idx);
        ASSERT(!elem_find(&a->desc->free_list, &b->free_node));
        list_append(&a->desc->free_list, &b->free_node);
      }
      intr_set_status(old_status);
    }

    // start alloc block
    b = elem2entry(MEM_BLOCK, free_node,
                   list_pop(&(descs[desc_idx].free_list)));
    memset(b, 0, descs[desc_idx].block_size);

    a = block2arena(b); // get arena addr
    a->cnt--;           // sub free_block numbers in arena
    lock_release(&mem_pool->lock);
    return (void *)b;
  }
}

/* MEMORY INIT ENTRY */
void mem_init() {
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));
  mem_pool_init(mem_bytes_total); // init memory pool
  block_desc_init(k_block_descs);
  put_str("mem_init done\n");
}

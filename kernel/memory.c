#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"

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
typedef struct _POOL
{
   struct bitmap pool_bitmap; // Bitmap. Control PYHCISAL MEMORY
   uint32_t phy_addr_start;   // PYHSICAL MEMORY START
   uint32_t pool_size;        // Byte size
} POOL, *PPOOL;

POOL kernel_pool, user_pool;
VISUAL_ADDRESS kernel_vaddr; // malloc for kernel

//malloc pg_cnt size pages in PF)
//return VA if success
//return NULL if fail
static void *vaddr_get(POOL_FLAGS pf, uint32_t pg_cnt)
{
   int vaddr_start = 0, bit_idx_start = -1;
   uint32_t cnt = 0;
   if (pf == PF_KERNEL)
   {
      bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
      if (bit_idx_start == -1)
      {
         return NULL;
      }
      while (cnt < pg_cnt)
      {
         bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
      }
      vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
   }
   else
   {
      //user memory pool
   }
   return (void *)vaddr_start;
}

//get PTE's va by input va
uint32_t *pte_ptr(uint32_t vaddr)
{
   //access physical addr by PDE 1023(WHICH point to PDE base physical addr)
   //offset by PDE to get PTE base addr(here cheat CPU with PTE bits)
   //offset by PTE to get PTE addr (here cheat CPU with OFFSET, so we should mul single PTE size)
   return (uint32_t *)(0xffc00000 +
                       ((vaddr & 0xffc00000) >> 10) +
                       PTE_IDX(vaddr) * 4);
}

//get PDE's va by input va
uint32_t *pde_ptr(uint32_t vaddr)
{
   // 0xFFFFF: get pa of PageTable
   return (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
};

//malloc one PhysicalPage in m_pool
//return page physical address if success
//return NULL if failed
static void *palloc(POOL *m_pool)
{
   // Ensure Atomic operation
   int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); // find page
   if (bit_idx == -1)
   {
      return NULL;
   }
   bitmap_set(&m_pool->pool_bitmap, bit_idx, 1); // set bit_idx 1, indicating used
   uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
   return (void *)page_phyaddr;
}

//add PTE mapping from _va to _pa
static void page_table_add(void *_vaddr, void *_page_phyaddr)
{
   uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
   uint32_t *pde = pde_ptr(vaddr);
   uint32_t *pte = pte_ptr(vaddr);

   if (*pde & 0x00000001)
   {                                                      // PDE&PDE 0bit P,indicating present
      ASSERT(!(*pte & 0x00000001));                       //this pte shouldn't present. otherwise PANIC("pte repeat!");
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
   }
   else
   { // 页目录项不存在,所以要先创建页目录再创建页表项.
      /* 页表中用到的页框一律从内核空间分配 */
      uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

      *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

      /* 分配到的物理页地址pde_phyaddr对应的物理内存清0,
       * 避免里面的陈旧数据变成了页表项,从而让页表混乱.
       * 访问到pde对应的物理地址,用pte取高20位便可.
       * 因为pte是基于该pde对应的物理地址内再寻址,
       * 把低12位置0便是该pde对应的物理页的起始*/
      memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE);

      ASSERT(!(*pte & 0x00000001));
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
   }
}

/* 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL */
void *malloc_page(POOL_FLAGS pf, uint32_t pg_cnt)
{
   ASSERT(pg_cnt > 0 && pg_cnt < 3840);
   /***********   malloc_page的原理是三个动作的合成:   ***********
      1通过vaddr_get在虚拟内存池中申请虚拟地址
      2通过palloc在物理内存池中申请物理页
      3通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
***************************************************************/
   void *vaddr_start = vaddr_get(pf, pg_cnt);
   if (vaddr_start == NULL)
   {
      return NULL;
   }

   uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
   PPOOL mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

   /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射*/
   while (cnt-- > 0)
   {
      void *page_phyaddr = palloc(mem_pool);
      if (page_phyaddr == NULL)
      { // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
         return NULL;
      }
      page_table_add((void *)vaddr, page_phyaddr); // 在页表中做映射
      vaddr += PG_SIZE;                            // 下一个虚拟页
   }
   return vaddr_start;
}

/* 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void *malloc_kernel_pages(uint32_t pg_cnt)
{
   void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
   if (vaddr != NULL)
   { // 若分配的地址不为空,将页框清0后返回
      memset(vaddr, 0, pg_cnt * PG_SIZE);
   }
   return vaddr;
}

/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem)
{
   put_str("   mem_pool_init start\n");
   uint32_t page_table_size = PG_SIZE * 256;       // 页表大小= 1页的页目录表+第0和第768个页目录项指向同一个页表+
                                                   // 第769~1022个页目录项共指向254个页表,共256个页框
   uint32_t used_mem = page_table_size + 0x100000; // 0x100000为低端1M内存
   uint32_t free_mem = all_mem - used_mem;
   uint16_t all_free_pages = free_mem / PG_SIZE; // 1页为4k,不管总内存是不是4k的倍数,
                                                 // 对于以页为单位的内存分配策略，不足1页的内存不用考虑了。
   uint16_t kernel_free_pages = all_free_pages / 2;
   uint16_t user_free_pages = all_free_pages - kernel_free_pages;

   /* 为简化位图操作，余数不处理，坏处是这样做会丢内存。
好处是不用做内存的越界检查,因为位图表示的内存少于实际物理内存*/
   uint32_t kbm_length = kernel_free_pages / 8; // Kernel BitMap的长度,位图中的一位表示一页,以字节为单位
   uint32_t ubm_length = user_free_pages / 8;   // User BitMap的长度.

   uint32_t kp_start = used_mem;                               // Kernel Pool start,内核内存池的起始地址
   uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE; // User Pool start,用户内存池的起始地址

   kernel_pool.phy_addr_start = kp_start;
   user_pool.phy_addr_start = up_start;

   kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
   user_pool.pool_size = user_free_pages * PG_SIZE;

   kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
   user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

   /*********    内核内存池和用户内存池位图   ***********
 *   位图是全局的数据，长度不固定。
 *   全局或静态的数组需要在编译时知道其长度，
 *   而我们需要根据总内存大小算出需要多少字节。
 *   所以改为指定一块内存来生成位图.
 *   ************************************************/
   // 内核使用的最高地址是0xc009f000,这是主线程的栈地址.(内核的大小预计为70K左右)
   // 32M内存占用的位图是2k.内核内存池的位图先定在MEM_BITMAP_BASE(0xc009a000)处.
   kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;

   /* 用户内存池的位图紧跟在内核内存池位图之后 */
   user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length);
   /******************** 输出内存池信息 **********************/
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

   /* 将位图置0*/
   bitmap_init(&kernel_pool.pool_bitmap);
   bitmap_init(&user_pool.pool_bitmap);

   /* 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。*/
   kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length; // 用于维护内核堆的虚拟地址,所以要和内核内存池大小一致

   /* 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外*/
   kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);

   kernel_vaddr.vaddr_start = K_HEAP_START;
   bitmap_init(&kernel_vaddr.vaddr_bitmap);
   put_str("   mem_pool_init done\n");
}

/* MEMORY INIT ENTRY */
void mem_init()
{
   put_str("mem_init start\n");
   uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));
   mem_pool_init(mem_bytes_total); // init memory pool
   put_str("mem_init done\n");
}

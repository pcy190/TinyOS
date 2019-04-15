#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"

/* 内存池标记,用于判断用哪个内存池 */
typedef enum _POOL_FLAGS
{
   PF_KERNEL = 1, // Kernel pool
   PF_USER = 2    // User pool
}POOL_FLAGS;

#define PG_P_1 1  // PageTable Present Bit
#define PG_P_0 0 
#define PG_RW_R 0 // R/W , Read/Exec
#define PG_RW_W 2 // R/W , Read/Write/Exec
#define PG_US_S 0 // U/S , System
#define PG_US_U 4 // U/S , User

/* 用于虚拟地址管理 */
typedef struct _VISUAL_ADDRESS
{
   /* 虚拟地址用到的位图结构，用于记录哪些虚拟地址被占用了。以页为单位。*/
   struct bitmap vaddr_bitmap;
   /* 管理的虚拟地址 */
   uint32_t vaddr_start;
} VISUAL_ADDRESS;

extern struct _POOL kernel_pool, user_pool;
void mem_init(void);
void *malloc_kernel_pages(uint32_t pg_cnt);
void *malloc_page(POOL_FLAGS pf, uint32_t pg_cnt);
void malloc_init(void);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);
#endif

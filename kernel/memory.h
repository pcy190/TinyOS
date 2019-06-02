#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

//memory pool
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


typedef struct _VISUAL_ADDRESS
{
   //bitmap record the VA is used or not. Page Size
   struct bitmap vaddr_bitmap;
   //virtual address
   uint32_t vaddr_start;
} VISUAL_ADDRESS;

typedef struct _MEM_BLOCK{
   LIST_NODE free_node;
}MEM_BLOCK,*PMEM_BLOCK;

typedef struct _MEM_BLOCK_DESC
{
   uint32_t block_size;
   uint32_t blocks_per_arena;
   LIST free_list;
}MEM_BLOCK_DESC,*PMEM_BLOCK_DESC;

#define DESC_CNT 7

extern struct _POOL kernel_pool, user_pool;
void mem_init(void);
void *get_kernel_pages(uint32_t pg_cnt);
void *malloc_page(POOL_FLAGS pf, uint32_t pg_cnt);
void malloc_init(void);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void* get_a_page(POOL_FLAGS pf, uint32_t vaddr);
void* get_user_pages(uint32_t pg_cnt);
void block_desc_init(PMEM_BLOCK_DESC desc_array);
void* sys_malloc(uint32_t size);
#endif

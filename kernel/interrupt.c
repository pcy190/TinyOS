#include "interrupt.h"
#include "global.h"
#include "io.h"
#include "print.h"
#include "stdint.h"

// USE PIC 8259A
#define PIC_M_CTRL 0x20 // master control port : 0x20
#define PIC_M_DATA 0x21 // master data port : 0x21
#define PIC_S_CTRL 0xa0 // slave control port : 0xa0
#define PIC_S_DATA 0xa1 // slave data port : 0xa1

#define IDT_DESC_CNT 0x81 // total IDT entry number

#define EFLAGS_IF 0x00000200 // eflags IF bit
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" \
                                           : "=g"(EFLAG_VAR))

extern uint32_t syscall_handler(void);

//Interrupt gate descriptor table
struct gate_desc
{
   uint16_t func_offset_low_word;
   uint16_t selector;
   uint8_t dwcount; // fixed, 4th byte in door desc
   uint8_t attribute;
   uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];

char *intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];               // interrupt handler entry table
extern intr_handler intr_entry_table[IDT_DESC_CNT]; // refer entry table in kernel.S file

/* init PIC 8259A */
static void pic_init(void)
{

   /* init master */
   outb(PIC_M_CTRL, 0x11); // ICW1: Edge triggered,cascade 8259, needICW4.
   outb(PIC_M_DATA, 0x20); // ICW2: start IVT number 0x20, e.g.IR[0-7] : 0x20 ~ 0x27.
   outb(PIC_M_DATA, 0x04); // ICW3: IR2 connect to slave.
   outb(PIC_M_DATA, 0x01); // ICW4: 8086 mode, EOI

   /* init slave */
   outb(PIC_S_CTRL, 0x11); // ICW1: Edge triggered,cascade 8259, needICW4.
   outb(PIC_S_DATA,
        0x28);             // ICW2: start IVT number 0x28, e.g. IR[8-15] : 0x28 ~ 0x2F
   outb(PIC_S_DATA, 0x02); // ICW3: set slave pin to connect to master IR2
   outb(PIC_S_DATA, 0x01); // ICW4: 8086 mode, EOI

   /* open master IR0, only accept IR0 (clock interrput ) */
   //allow key intrrupt
   outb(PIC_M_DATA, 0xf8);//FE:only IR0; FD:only IR1 ;FC:both IR0&1 ; F8: IRQ0 & IRQ1 & slave IRQ2
   outb(PIC_S_DATA, 0xbf); //FF： close ; BF: open slave IRQ14(harddisk)

   put_str("   pic_init done\n");
}

//init idt 
static void idt_desc_init(void) {
   int i, lastindex = IDT_DESC_CNT - 1;
   for (i = 0; i < IDT_DESC_CNT; i++) {
      make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]); 
   }
   //sys handler's dpl:3,
   make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
   put_str("   idt_desc_init done\n");
}

/* init & make idt descriptor */
static void make_idt_desc(struct gate_desc *p_gdesc, uint8_t attr,
                          intr_handler function)
{
   p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
   p_gdesc->selector = SELECTOR_K_CODE;
   p_gdesc->dwcount = 0;
   p_gdesc->attribute = attr;
   p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

static void general_intr_handler(uint8_t vec_nr)
{
   if (vec_nr == 0x27 || vec_nr == 0x2f)
   {
      return; // IRQ7 & IRQ15 produce spurious interrupt, ignore it
   }
   set_cursor(0);
   int cursor_pos = 0;
   while (cursor_pos < 320)
   {
      put_char(' ');
      cursor_pos++;
   }
   set_cursor(0);
   put_str("**************! EXCEPTION !*******************\n");
   set_cursor(88);
   put_str(intr_name[vec_nr]);
   if (vec_nr == 14)
   {
      //PageFalut
      int pf_vaddr = 0;
      asm("movl %%cr2,%0"
          : "=r"(pf_vaddr));
      put_str("Page Fault At 0x");
      put_int(pf_vaddr);
   }

   put_str("INT vector: 0x");
   put_int(vec_nr);
   put_char('\n');
   put_str("**************! EXCEPTION MESSAGE END !*******************\n");
   while (1)
      ;
}

static void exception_init(void)
{ // reg gengeral handler
   int i;
   // idt_table function called by kernel/kernel.S [idt_table + %1*4]
   for (i = 0; i < IDT_DESC_CNT; i++)
   {
      idt_table[i] = general_intr_handler; // default general_intr_handler。
      intr_name[i] = "unknown";
   }
   intr_name[0] = "#DE Divide Error";
   intr_name[1] = "#DB Debug Exception";
   intr_name[2] = "NMI Interrupt";
   intr_name[3] = "#BP Breakpoint Exception";
   intr_name[4] = "#OF Overflow Exception";
   intr_name[5] = "#BR BOUND Range Exceeded Exception";
   intr_name[6] = "#UD Invalid Opcode Exception";
   intr_name[7] = "#NM Device Not Available Exception";
   intr_name[8] = "#DF Double Fault Exception";
   intr_name[9] = "Coprocessor Segment Overrun";
   intr_name[10] = "#TS Invalid TSS Exception";
   intr_name[11] = "#NP Segment Not Present";
   intr_name[12] = "#SS Stack Fault Exception";
   intr_name[13] = "#GP General Protection Exception";
   intr_name[14] = "#PF Page-Fault Exception";
   // intr_name[15] reserved.
   intr_name[16] = "#MF x87 FPU Floating-Point Error";
   intr_name[17] = "#AC Alignment Check Exception";
   intr_name[18] = "#MC Machine-Check Exception";
   intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/* sti , return previous status*/
INTR_STATUS intr_enable()
{
   INTR_STATUS old_status;
   if (INTR_ON == intr_get_status())
   {
      old_status = INTR_ON;
      return old_status;
   }
   else
   {
      old_status = INTR_OFF;
      asm volatile("sti");
      return old_status;
   }
}

/* cli , return previous status*/
INTR_STATUS intr_disable()
{
   INTR_STATUS old_status;
   if (INTR_ON == intr_get_status())
   {
      old_status = INTR_ON;
      asm volatile("cli"
                   :
                   :
                   : "memory");
   }
   else
   {
      old_status = INTR_OFF;
   }
   return old_status;
}

/* set INTERRUPT STATUS as status */
INTR_STATUS intr_set_status(INTR_STATUS status)
{
   return status & INTR_ON ? intr_enable() : intr_disable();
}

/* get current status */
INTR_STATUS intr_get_status()
{
   uint32_t eflags = 0;
   GET_EFLAGS(eflags);
   return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

//register function on vector_index of VECTOR
void register_handler(uint8_t vector_index, intr_handler function)
{
   idt_table[vector_index] = function;
}

/* init INTERRUPT work*/
void idt_init()
{
   put_str("idt_init start\n");
   idt_desc_init(); //   init idt
   exception_init();
   pic_init(); //   init 8259A

   /* load idt */
   uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
   asm volatile("lidt %0"
                :
                : "m"(idt_operand));
   put_str("idt_init done\n");
}

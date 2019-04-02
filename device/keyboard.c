#include "keyboard.h"
#include "global.h"
#include "interrupt.h"
#include "io.h"
#include "ioqueue.h"
#include "print.h"

#define KBD_BUF_PORT 0x60 // kbd buffer register port: 0x60

#define esc '\033'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'

#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

// breakcode and makecode
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a

IOQUEUE kbd_buf;

/* recode key status,
 * ext_scancode record makecode 0xe0 symbols */
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

/* make_code index table */
static char keymap[][2] = {
    /* scancode   non-shift  with-shift*/
    /* ---------------------------------- */
    /* 0x00 */ {0, 0},
    /* 0x01 */ {esc, esc},
    /* 0x02 */ {'1', '!'},
    /* 0x03 */ {'2', '@'},
    /* 0x04 */ {'3', '#'},
    /* 0x05 */ {'4', '$'},
    /* 0x06 */ {'5', '%'},
    /* 0x07 */ {'6', '^'},
    /* 0x08 */ {'7', '&'},
    /* 0x09 */ {'8', '*'},
    /* 0x0A */ {'9', '('},
    /* 0x0B */ {'0', ')'},
    /* 0x0C */ {'-', '_'},
    /* 0x0D */ {'=', '+'},
    /* 0x0E */ {backspace, backspace},
    /* 0x0F */ {tab, tab},
    /* 0x10 */ {'q', 'Q'},
    /* 0x11 */ {'w', 'W'},
    /* 0x12 */ {'e', 'E'},
    /* 0x13 */ {'r', 'R'},
    /* 0x14 */ {'t', 'T'},
    /* 0x15 */ {'y', 'Y'},
    /* 0x16 */ {'u', 'U'},
    /* 0x17 */ {'i', 'I'},
    /* 0x18 */ {'o', 'O'},
    /* 0x19 */ {'p', 'P'},
    /* 0x1A */ {'[', '{'},
    /* 0x1B */ {']', '}'},
    /* 0x1C */ {enter, enter},
    /* 0x1D */ {ctrl_l_char, ctrl_l_char},
    /* 0x1E */ {'a', 'A'},
    /* 0x1F */ {'s', 'S'},
    /* 0x20 */ {'d', 'D'},
    /* 0x21 */ {'f', 'F'},
    /* 0x22 */ {'g', 'G'},
    /* 0x23 */ {'h', 'H'},
    /* 0x24 */ {'j', 'J'},
    /* 0x25 */ {'k', 'K'},
    /* 0x26 */ {'l', 'L'},
    /* 0x27 */ {';', ':'},
    /* 0x28 */ {'\'', '"'},
    /* 0x29 */ {'`', '~'},
    /* 0x2A */ {shift_l_char, shift_l_char},
    /* 0x2B */ {'\\', '|'},
    /* 0x2C */ {'z', 'Z'},
    /* 0x2D */ {'x', 'X'},
    /* 0x2E */ {'c', 'C'},
    /* 0x2F */ {'v', 'V'},
    /* 0x30 */ {'b', 'B'},
    /* 0x31 */ {'n', 'N'},
    /* 0x32 */ {'m', 'M'},
    /* 0x33 */ {',', '<'},
    /* 0x34 */ {'.', '>'},
    /* 0x35 */ {'/', '?'},
    /* 0x36	*/ {shift_r_char, shift_r_char},
    /* 0x37 */ {'*', '*'},
    /* 0x38 */ {alt_l_char, alt_l_char},
    /* 0x39 */ {' ', ' '},
    /* 0x3A */ {caps_lock_char, caps_lock_char}};

// handler
static void intr_keyboard_handler(void)
{
    bool ctrl_down_last = ctrl_status;
    bool shift_down_last = shift_status;
    bool caps_lock_last = caps_lock_status;

    bool break_code;
    uint16_t scancode = inb(KBD_BUF_PORT);

    // if starts with 0xe0,then wait next intr
    if (scancode == 0xe0)
    {
        ext_scancode = true; // open 0xe0 sym
        return;
    }

    // combine scancode
    if (ext_scancode)
    {
        scancode = ((0xe000) | scancode);
        ext_scancode = false; // close 0xe0 sym
    }

    break_code = ((scancode & 0x0080) != 0); // break_code flag

    if (break_code)
    {

        uint16_t make_code = (scancode &= 0xff7f);

        if (make_code == ctrl_l_make || make_code == ctrl_r_make)
        {
            ctrl_status = false;
        }
        else if (make_code == shift_l_make || make_code == shift_r_make)
        {
            shift_status = false;
        }
        else if (make_code == alt_l_make || make_code == alt_r_make)
        {
            alt_status = false;
        }

        return;
    }
    // makecode
    else if ((scancode > 0x00 && scancode < 0x3b) || (scancode == alt_r_make) ||
             (scancode == ctrl_r_make))
    {
        bool shift = false;
        if ((scancode < 0x0e) || (scancode == 0x29) || (scancode == 0x1a) ||
            (scancode == 0x1b) || (scancode == 0x2b) || (scancode == 0x27) ||
            (scancode == 0x28) || (scancode == 0x33) || (scancode == 0x34) ||
            (scancode == 0x35))
        {
            /*******************
               0x0-0x0e '0'~'9','-','='
               0x29 '`'
               0x1a '['
               0x1b ']'
               0x2b '\\'
               0x27 ';'
               0x28 '\''
               0x33 ','
               0x34 '.'
               0x35 '/'
          *******************************/
            if (shift_down_last)
            { //shift down
                shift = true;
            }
        }
        else
        { // judge shift and capslock
            if (shift_down_last && caps_lock_last)
            {
                shift = false;
            }
            else if (shift_down_last ||
                     caps_lock_last)
            {
                shift = true;
            }
            else
            {
                shift = false;
            }
        }

        uint8_t index = (scancode &= 0x00ff);
        char cur_char = keymap[index][shift]; // index char

        if (cur_char)
        {
            //Shortcut keys: ctrl+l ctrl+u
            if ((ctrl_down_last && cur_char == 'l') ||
                (ctrl_down_last && cur_char == 'u'))
            {
                cur_char -= 'a';
            }
            if (!ioq_full(&kbd_buf))
            {
                put_char(cur_char); // TMP SHOW------------------------------------
                ioq_putchar(&kbd_buf, cur_char);
            }
            return;
        }

        //recoard command key status
        if (scancode == ctrl_l_make || scancode == ctrl_r_make)
        {
            ctrl_status = true;
        }
        else if (scancode == shift_l_make || scancode == shift_r_make)
        {
            shift_status = true;
        }
        else if (scancode == alt_l_make || scancode == alt_r_make)
        {
            alt_status = true;
        }
        else if (scancode == caps_lock_make)
        {
            //caps_lock status
            caps_lock_status = !caps_lock_status;
        }
    }
    else
    {
        put_str("unkonwn\n");
    }
}

// init
void keyboard_init()
{
    put_str("keyboard init start\n");
    ioqueue_init(&kbd_buf);
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard init done\n");
}

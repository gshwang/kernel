/*
 * ep93xx_keypad.h Keymap definitions
 */

#if defined(CONFIG_KEYBOARD_EP93XX_SCROLL_WHEEL)
#define SCROLL_WHEEL_KEY_SUPPORT
#endif

#ifdef SCROLL_WHEEL_KEY_SUPPORT

/*
 *  Support for a scroll wheel key:
 *      ALPS 12mm Size Insulated Shaft Type Encoder.
 *  Part NO. :      EC12E2424404
 *  Connect TO:     Col1/2-Row3
 */
#define FIX_FOR_EC12E2424404 1


#define KEY_WHEEL_1      KEY_UP
#define KEY_WHEEL_2      KEY_DOWN

static int wheel_phase_table[16] = {1,0,2,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};


#endif


//
// This table is used to map the scan code to the Linux default keymap.
//
#define KeyPad_SIZE         (8*8)
static unsigned int const KeyPadTable[KeyPad_SIZE] = 
{
/*             COL0        COL1        CLO2        COL3       COL4       COL5       COL6       COL7   */
/*ROW0*/       KEY_ESC,    KEY_1 ,     KEY_2 ,     0,         0,         0,         0,         0, 
/*ROW1*/       KEY_3 ,     KEY_4 ,     KEY_5 ,     0,         0,         0,         0,         0, 
/*ROW2*/       KEY_6 ,     KEY_7 ,     KEY_ENTER,  0,         0,         0,         0,         0, 
/*ROW3*/       0,          KEY_UP,     KEY_DOWN,   0,         0,         0,         0,         0, 
/*ROW4*/       0,          0,          0,          0,         0,         0,         0,         0, 
/*ROW5*/       0,          0,          0,          0,         0,         0,         0,         0, 
/*ROW6*/       0,          0,          0,          0,         0,         0,         0,         0, 
/*ROW7*/       0,          0,          0,          0,         0,         0,         0,         0
};

struct ep93xx_keypad_dev
{
    struct input_dev * input;

#ifdef SCROLL_WHEEL_KEY_SUPPORT

#define WHEEL_FLAG1   1
#define WHEEL_FLAG2  2
    int wheelkey;
    int last_wheel_step;

#ifdef FIX_FOR_EC12E2424404
    int wheelkey_buffer[4];
    int wheelkey_count;
    int wheelkey_hit;
#endif

#endif

    int last_key1;
    int last_key2;

};

#define SCANINIT_PRSCL_MASK         0x000003FF
#define SCANINIT_PRSCL_SHIFT        0L
#define SCANINIT_T2                 0x00001000
#define SCANINIT_BACK               0x00002000
#define SCANINIT_DIAG               0x00004000
#define SCANINIT_DIS3KY             0x00008000
#define SCANINIT_DBNC_MASK          0x00FF0000
#define SCANINIT_DBNC_SHIFT         16L

#define KEYREG_KEY1_MASK            0x0000003F
#define KEYREG_KEY1_SHIFT           0L
#define KEYREG_KEY2_MASK            0x00000Fc0
#define KEYREG_KEY2_SHIFT           6L
#define KEYREG_KEY1ROW_MASK         0x00000007
#define KEYREG_KEY1ROW_SHIFT        0L
#define KEYREG_KEY1COL_MASK         0x00000038
#define KEYREG_KEY1COL_SHIFT        3L
#define KEYREG_KEY2ROW_MASK         0x000001c0
#define KEYREG_KEY2ROW_SHIFT        6L
#define KEYREG_KEY2COL_MASK         0x00000E00
#define KEYREG_KEY2COL_SHIFT        9L

#define KEYREG_KEY1                 0x00001000
#define KEYREG_KEY2                 0x00002000
#define KEYREG_INT                  0x00004000
#define KEYREG_K                    0x00008000

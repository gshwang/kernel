/*
 * ep93xx_spi_kbd.h Keymap definitions (to be cleaned up)
 */

#define KSCAN_TABLE_SIZE    0x88

#define EXTENDED_KEY     0x80

//
// This table is used to map the scan code to the Linux default keymap.
//
static unsigned int const KScanCodeToVKeyTable[KSCAN_TABLE_SIZE] = {
	KEY_RESERVED,		// Scan Code 0x0
	KEY_F9,			// Scan Code 0x1
	KEY_RESERVED,		// Scan Code 0x2
	KEY_F5,			// Scan Code 0x3
	KEY_F3,			// Scan Code 0x4
	KEY_F1,			// Scan Code 0x5
	KEY_F2,			// Scan Code 0x6
	KEY_F12,		// Scan Code 0x7
	KEY_RESERVED,		// Scan Code 0x8
	KEY_F10,		// Scan Code 0x9
	KEY_F8,			// Scan Code 0xA
	KEY_F6,			// Scan Code 0xB
	KEY_F4,			// Scan Code 0xC
	KEY_TAB,		// Scan Code 0xD Tab
	KEY_GRAVE,		// Scan Code 0xE '
	KEY_RESERVED,		// Scan Code 0xF
	KEY_RESERVED,		// Scan Code 0x10
	KEY_LEFTALT,		// Scan Code 0x11 Left Menu
	KEY_LEFTSHIFT,		// Scan Code 0x12 Left Shift
	KEY_RESERVED,		// Scan Code 0x13
	KEY_LEFTCTRL,		// Scan Code 0x14
	KEY_Q,			// Scan Code 0x15
	KEY_1,			// Scan Code 0x16
	KEY_RESERVED,		// Scan Code 0x17
	KEY_RESERVED,		// Scan Code 0x18
	KEY_RESERVED,		// Scan Code 0x19
	KEY_Z,			// Scan Code 0x1A
	KEY_S,			// Scan Code 0x1B
	KEY_A,			// Scan Code 0x1C
	KEY_W,			// Scan Code 0x1D
	KEY_2,			// Scan Code 0x1E
	KEY_RESERVED,		// Scan Code 0x1F
	KEY_RESERVED,		// Scan Code 0x20
	KEY_C,			// Scan Code 0x21
	KEY_X,			// Scan Code 0x22
	KEY_D,			// Scan Code 0x23
	KEY_E,			// Scan Code 0x24
	KEY_4,			// Scan Code 0x25
	KEY_3,			// Scan Code 0x26
	KEY_RESERVED,		// Scan Code 0x27
	KEY_RESERVED,		// Scan Code 0x28
	KEY_SPACE,		// Scan Code 0x29  Space
	KEY_V,			// Scan Code 0x2A
	KEY_F,			// Scan Code 0x2B
	KEY_T,			// Scan Code 0x2C
	KEY_R,			// Scan Code 0x2D
	KEY_5,			// Scan Code 0x2E
	KEY_RESERVED,		// Scan Code 0x2F
	KEY_RESERVED,		// Scan Code 0x30
	KEY_N,			// Scan Code 0x31
	KEY_B,			// Scan Code 0x32 B
	KEY_H,			// Scan Code 0x33
	KEY_G,			// Scan Code 0x34
	KEY_Y,			// Scan Code 0x35
	KEY_6,			// Scan Code 0x36
	KEY_RESERVED,		// Scan Code 0x37
	KEY_RESERVED,		// Scan Code 0x38
	KEY_RESERVED,		// Scan Code 0x39
	KEY_M,			// Scan Code 0x3A
	KEY_J,			// Scan Code 0x3B
	KEY_U,			// Scan Code 0x3C
	KEY_7,			// Scan Code 0x3D
	KEY_8,			// Scan Code 0x3E
	KEY_RESERVED,		// Scan Code 0x3F
	KEY_RESERVED,		// Scan Code 0x40
	KEY_COMMA,		// Scan Code 0x41
	KEY_K,			// Scan Code 0x42
	KEY_I,			// Scan Code 0x43       
	KEY_O,			// Scan Code 0x44
	KEY_0,			// Scan Code 0x45
	KEY_9,			// Scan Code 0x46
	KEY_RESERVED,		// Scan Code 0x47
	KEY_RESERVED,		// Scan Code 0x48
	KEY_DOT,		// Scan Code 0x49
	KEY_SLASH,		// Scan Code 0x4A
	KEY_L,			// Scan Code 0x4B
	KEY_SEMICOLON,		// Scan Code 0x4C
	KEY_P,			// Scan Code 0x4D
	KEY_MINUS,		// Scan Code 0x4E
	KEY_RESERVED,		// Scan Code 0x4F
	KEY_RESERVED,		// Scan Code 0x50
	KEY_RESERVED,		// Scan Code 0x51
	KEY_APOSTROPHE,		// Scan Code 0x52
	KEY_RESERVED,		// Scan Code 0x53
	KEY_LEFTBRACE,		// Scan Code 0x54
	KEY_EQUAL,		// Scan Code 0x55
	KEY_BACKSPACE,		// Scan Code 0x56
	KEY_RESERVED,		// Scan Code 0x57
	KEY_CAPSLOCK,		// Scan Code 0x58 Caps Lock
	KEY_RIGHTSHIFT,		// Scan Code 0x59 Right Shift
	KEY_ENTER,		// Scan Code 0x5A
	KEY_RIGHTBRACE,		// Scan Code 0x5B
	KEY_RESERVED,		// Scan Code 0x5C
	KEY_BACKSLASH,		// Scan Code 0x5D
	KEY_RESERVED,		// Scan Code 0x5E
	KEY_RESERVED,		// Scan Code 0x5F
	KEY_RESERVED,		// Scan Code 0x60
	KEY_BACKSLASH,		// Scan Code 0x61 ?? //VK_BSLH,            
	KEY_RESERVED,		// Scan Code 0x62
	KEY_RESERVED,		// Scan Code 0x63
	KEY_RESERVED,		// Scan Code 0x64
	KEY_RESERVED,		// Scan Code 0x65
	KEY_BACKSPACE,		// Scan Code 0x66 ?? //VK_BKSP,            
	KEY_RESERVED,		// Scan Code 0x67
	KEY_RESERVED,		// Scan Code 0x68
	KEY_KP1,		// Scan Code 0x69
	KEY_RESERVED,		// Scan Code 0x6A
	KEY_KP4,		// Scan Code 0x6B
	KEY_KP7,		// Scan Code 0x6C
	KEY_RESERVED,		// Scan Code 0x6D
	KEY_RESERVED,		// Scan Code 0x6E
	KEY_RESERVED,		// Scan Code 0x6F
	KEY_KP0,		// Scan Code 0x70
	KEY_KPDOT,		// Scan Code 0x71 DECIMAL??
	KEY_KP2,		// Scan Code 0x72
	KEY_KP5,		// Scan Code 0x73
	KEY_KP6,		// Scan Code 0x74
	KEY_KP8,		// Scan Code 0x75
	KEY_ESC,		// Scan Code 0x76
	KEY_NUMLOCK,		// Scan Code 0x77
	KEY_F11,		// Scan Code 0x78
	KEY_KPPLUS,		// Scan Code 0x79
	KEY_KP3,		// Scan Code 0x7A
	KEY_KPMINUS,		// Scan Code 0x7B
	KEY_KPASTERISK,		// Scan Code 0x7C
	KEY_KP9,		// Scan Code 0x7D
	KEY_SCROLLLOCK,		// Scan Code 0x7E
	KEY_RESERVED,		// Scan Code 0x7F
	KEY_RESERVED,		// Scan Code 0x80      
	KEY_RESERVED,		// Scan Code 0x81
	KEY_RESERVED,		// Scan Code 0x82
	KEY_F7,			// Scan Code 0x83
	KEY_RESERVED,		// Scan Code 0x84
	KEY_RESERVED,		// Scan Code 0x85
	KEY_RESERVED,		// Scan Code 0x86
	KEY_RESERVED		// Scan Code 0x87
};

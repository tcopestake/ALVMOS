#include <stdint.h>

#define VGA_COLOUR_BLACK (vga_terminal_colour) 0
#define VGA_COLOUR_GREY (vga_terminal_colour) 7

#define VGA_TERMINAL_CHAR_COLOURS(foreground, background) (foreground | (background << 4))

#define VGA_TERMINAL_CHAR_DEFAULT VGA_TERMINAL_CHAR_COLOURS(VGA_COLOUR_GREY, VGA_COLOUR_BLACK)

typedef uint8_t vga_terminal_colour;

void terminal_clear();

void terminal_print(char* text_string_ptr);

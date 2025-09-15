#include <stdint.h>
#include "terminal.h"

#define TEXT_VIDEO_MEMORY_POINTER (text_video_memory_ptr*) 0xB8000

typedef volatile uint8_t text_video_memory_ptr;

text_video_memory_ptr* text_next_byte_ptr = TEXT_VIDEO_MEMORY_POINTER;

void terminal_clear()
{
    text_next_byte_ptr = TEXT_VIDEO_MEMORY_POINTER;

    for (int i = 0; i < (80 * 25); i++) {
        *text_next_byte_ptr = ' ';
        ++text_next_byte_ptr;

        *text_next_byte_ptr = VGA_TERMINAL_CHAR_DEFAULT;
        ++text_next_byte_ptr;
    }

    text_next_byte_ptr = TEXT_VIDEO_MEMORY_POINTER;
}

void terminal_print(char* text_string_ptr)
{
    while (*text_string_ptr != 0x00) {
        *text_next_byte_ptr = *text_string_ptr;
        ++text_next_byte_ptr;
        
        *text_next_byte_ptr = VGA_TERMINAL_CHAR_DEFAULT;
        ++text_next_byte_ptr;

        ++text_string_ptr;
    }
}

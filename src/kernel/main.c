
#include <stdint.h>
#include "terminal/terminal.h"

typedef struct boot_state_struct {
    uint16_t memory_map_offset;
    uint16_t memory_map_size;
    uint16_t page_mapping_offset;
} boot_state_struct;

void kernel_main(boot_state_struct* boot_state)
{
    terminal_clear();
    terminal_print("Testing...");
}

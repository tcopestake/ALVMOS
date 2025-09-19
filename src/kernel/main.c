
#include <stdint.h>
#include "terminal/terminal.h"
#include "memmgmt/mapping.h"

typedef struct boot_state_struct {
    uint16_t memory_map_offset;
    uint16_t memory_map_size;
    uint16_t page_mapping_offset;
} boot_state_struct;

void kernel_main(boot_state_struct* boot_state)
{
    terminal_clear();
    terminal_print("Setting up ALVM kernel...");

    mem_mgmt_init(
        boot_state->page_mapping_offset,
        boot_state->memory_map_offset,
        boot_state->memory_map_size
    );
}

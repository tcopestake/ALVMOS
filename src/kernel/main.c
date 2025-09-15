
#include <stdint.h>

#define __intel(args...) asm volatile (".intel_syntax noprefix; " args);

typedef struct boot_state_struct {
    uint16_t memory_map_offset;
    uint16_t memory_map_size;
    uint16_t page_mapping_offset;
} boot_state_struct;

void kernel_main(boot_state_struct* boot_state)
{
    __intel("hlt");
}

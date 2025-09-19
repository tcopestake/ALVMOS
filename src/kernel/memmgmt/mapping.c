#include <stdint.h>
#include "mapping.h"
#include "../misc.h"

// @todo Process-specific memory allocation
// @todo Integrate with (or improve) the bootloader page tables.

#define MEMORY_PAGE_SIZE_BYTES 4096
#define LONGMODE_PAGE_TABLE_ENTRY_COUNT 512

typedef struct longmode_paging_level_4_entry {
    uint64_t todo;
} longmode_paging_level_4_entry;

typedef struct longmode_paging_pointer_table_entry {
    uint64_t todo;
} longmode_paging_pointer_table_entry;

typedef struct longmode_paging_directory_entry {
    uint64_t todo;
} longmode_paging_directory_entry;

typedef struct longmode_paging_table_entry {
    uint64_t todo;
} longmode_paging_table_entry;

typedef struct bootloader_longmode_page_tables {
    longmode_paging_level_4_entry level_4_entries[LONGMODE_PAGE_TABLE_ENTRY_COUNT];
    longmode_paging_pointer_table_entry pointer_table_entries[LONGMODE_PAGE_TABLE_ENTRY_COUNT];
    longmode_paging_directory_entry directory_entries[LONGMODE_PAGE_TABLE_ENTRY_COUNT];
    longmode_paging_table_entry table_entries[LONGMODE_PAGE_TABLE_ENTRY_COUNT];
} bootloader_longmode_page_tables;

/**
 * These values are defined by the BIOS memory mapping; they
 * (perhaps obviously) describe the type/state of a given region.
 */
enum bios_memory_type_enum {
    BIOS_MEMORY_REGION_USABLE = 1,
    BIOS_MEMORY_REGION_RESERVED = 2,
    BIOS_MEMORY_REGION_ACPI_RECLAIMABLE = 3,
    BIOS_MEMORY_REGION_ACPI_NVS = 4,
    BIOS_MEMORY_REGION_BAD = 5,
};

/**
 * This data structure is defined by the BIOS. (...)
 */
typedef union extended_attributes_def {
    uint32_t extended_attributes;
    struct {
        uint32_t ignore_bit: 1;
        uint32_t non_volatile_bit: 1;
        uint32_t reserved: 30;
    };
} extended_attributes_def;

/**
 * This data structure is defined by the BIOS. It describes
 * a region of memory (start address, size in bytes, type, etc.).
 */
typedef struct bios_memory_map_node {
    uint64_t base_address;
    uint64_t length;
    enum bios_memory_type_enum type;
    extended_attributes_def extended_attributes;
} bios_memory_map_node;

typedef struct memory_region memory_region;
typedef struct memory_page memory_page;
typedef struct memory_allocation_unit memory_allocation_unit;

/**
 * This structure is used to manange/describe regions of memory
 * and the free/allocated pages within.
 */
typedef struct memory_region {
    uint8_t* base_address;
    uint64_t total_page_count;
    uint64_t free_page_count;
    memory_region* next_region;
    memory_page* first_page;
    memory_page* next_free_page;
} memory_region;

/**
 * This structure is used to manange/describe discrete pages.
 */
typedef struct memory_page {
    memory_region* region;
    uint64_t base_address;
    memory_page* previous;
    memory_page* next;
} memory_page;

/**
 * This structure is used to manange/describe a discrete area of
 * memory allocated within a page (e.g. by calling malloc).
 */
typedef struct memory_allocation_unit {
    uint8_t* base_address;
    uint64_t size_bytes;
    memory_page* page;
    memory_allocation_unit* left;
    memory_allocation_unit* right;
} memory_allocation_unit;

// Until we have functional dynamic memory allocation, we'll
// have to allocate some memory here for these initial structs/pointers.

memory_region root_memory_region;
memory_region* root_memory_region_ptr = NULL_POINTER;
memory_region* last_memory_region_ptr = NULL_POINTER;
memory_page first_page;

bootloader_longmode_page_tables* bootloader_longmode_page_tables_ptr;

void mem_mgmt_init(
    uint16_t page_mapping_offset,
    uint16_t memory_map_offset,
    uint16_t memory_map_size
) {
    bootloader_longmode_page_tables_ptr = (bootloader_longmode_page_tables*) page_mapping_offset;

    mem_mgmt_init_parse_memory_map((bios_memory_map_node*) memory_map_offset, memory_map_size);
}

mem_mgmt_init_parse_memory_map(bios_memory_map_node* memory_map_ptr, uint16_t memory_map_size)
{
    for (uint16_t i = 0; i < memory_map_size; i++) {
        if (memory_map_ptr->type == BIOS_MEMORY_REGION_USABLE) {
            mem_mgmt_init_handle_usable_region(memory_map_ptr);
        }

        ++memory_map_ptr;
    }
}

void mem_mgmt_init_handle_usable_region(bios_memory_map_node* free_region_ptr)
{
    // If the region is smaller than 1 page in size, we'll ignore it for now.

    if (free_region_ptr->length < MEMORY_PAGE_SIZE_BYTES) {
        return;
    }

    // If this is the first decent region we've found, we'll use the memory
    // already allocated for root_memory_region. Otherwise, we'll allocate
    // new memory and add the region to the list.

    if (root_memory_region_ptr == NULL_POINTER) {
        mem_mgmt_init_handle_first_region(free_region_ptr);
    } else {
        mem_mgmt_init_handle_subsequent_region(free_region_ptr);
    }
}

void mem_mgmt_init_handle_first_region(bios_memory_map_node* free_region_ptr)
{
    // Update root_memory_region and the root_memory_region pointer.

    mem_mgmt_init_setup_region(free_region_ptr, &root_memory_region);

    root_memory_region_ptr = &root_memory_region;
    last_memory_region_ptr = root_memory_region_ptr;

    // Describe/map the first page.

    // @todo
}

void mem_mgmt_init_handle_subsequent_region(bios_memory_map_node* free_region_ptr)
{
    // Allocate memory for a new memory_region struct & populate it.

    memory_region* new_memory_region = mem_mgmt_alloc(sizeof(memory_region));

    mem_mgmt_init_setup_region(free_region_ptr, new_memory_region);

    // Add the new struct to the list.

    last_memory_region_ptr->next_region = new_memory_region;

    last_memory_region_ptr = new_memory_region;
}

void mem_mgmt_init_setup_region(bios_memory_map_node* free_region_ptr, memory_region* new_memory_region)
{
    new_memory_region->base_address = free_region_ptr->base_address;
    new_memory_region->total_page_count = (free_region_ptr->length / MEMORY_PAGE_SIZE_BYTES);
    new_memory_region->free_page_count = root_memory_region.total_page_count;
    new_memory_region->next_region = NULL_POINTER;
}

/**
 * (Not to be confused with userspace malloc.)
 * @todo
 */
uint8_t* mem_mgmt_alloc(uint64_t bytes)
{
    // If there's a mapped page with enough space, use it.
    // Otherwise, find & map a new page.
}

/**
 * (Not to be confused with userspace free.)
 * @todo
 */
void mem_mgmt_free(uint8_t* pointer)
{

}

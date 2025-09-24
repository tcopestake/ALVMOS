#include <stdint.h>
#include "mapping.h"
#include "../misc.h"

// @todo mark structures and such as volatile
// @todo Process-specific memory allocation
// @todo Integrate with (or improve) the bootloader page tables.

/**
 * Starting all virtual memory at 0x1000000 should avoid conflicts with
 * all the random shit already mapped and reserved and such.
 */
#define VIRTUAL_MEMORY_START 0x1000000

#define MEMORY_PAGE_SIZE_BYTES 4096

#define LONGMODE_PAGING_ENTRY_COUNT 512

#define LONGMODE_PAGING_LEVEL4_PRESENT_BIT_MASK 0x1
#define LONGMODE_PAGING_LEVEL4_READWRITE_BIT_MASK (0x1 << 1)
#define LONGMODE_PAGING_LEVEL4_USER_SUPERVISOR_BIT_MASK (0x1 << 2)
#define LONGMODE_PAGING_LEVEL4_WRITETHROUGH_BIT_MASK (0x1 << 3)
#define LONGMODE_PAGING_LEVEL4_CACHE_DISABLE_BIT_MASK (0x1 << 4)
#define LONGMODE_PAGING_LEVEL4_ACCESSED_BIT_MASK (0x1 << 5)
#define LONGMODE_PAGING_LEVEL4_ADDRESS_MASK (0xFFFFFFFFFF << 12)
#define LONGMODE_PAGING_LEVEL4_EXECUTE_DISABLE_BIT_MASK (0x1 << 63)

#define LONGMODE_PAGING_POINTER_TABLE_PRESENT_BIT_MASK 0x1
#define LONGMODE_PAGING_POINTER_TABLE_READWRITE_BIT_MASK (0x1 << 1)
#define LONGMODE_PAGING_POINTER_TABLE_USER_SUPERVISOR_BIT_MASK (0x1 << 2)
#define LONGMODE_PAGING_POINTER_TABLE_WRITETHROUGH_BIT_MASK (0x1 << 3)
#define LONGMODE_PAGING_POINTER_TABLE_CACHE_DISABLE_BIT_MASK (0x1 << 4)
#define LONGMODE_PAGING_POINTER_TABLE_ACCESSED_BIT_MASK (0x1 << 5)
#define LONGMODE_PAGING_POINTER_TABLE_PAGE_SIZE_BIT_MASK (0x1 << 7)
#define LONGMODE_PAGING_POINTER_TABLE_ADDRESS_MASK (0xFFFFFFFFFF << 12)
#define LONGMODE_PAGING_POINTER_TABLE_EXECUTE_DISABLE_BIT_MASK (0x1 << 63)

#define LONGMODE_PAGING_TABLE_ENTRY_PRESENT_BIT_MASK 0x1
#define LONGMODE_PAGING_TABLE_ENTRY_READWRITE_BIT_MASK (0x1 << 1)
#define LONGMODE_PAGING_TABLE_ENTRY_USER_SUPERVISOR_BIT_MASK (0x1 << 2)
#define LONGMODE_PAGING_TABLE_ENTRY_WRITETHROUGH_BIT_MASK (0x1 << 3)
#define LONGMODE_PAGING_TABLE_ENTRY_CACHE_DISABLE_BIT_MASK (0x1 << 4)
#define LONGMODE_PAGING_TABLE_ENTRY_ACCESSED_BIT_MASK (0x1 << 5)
#define LONGMODE_PAGING_TABLE_ENTRY_DIRTY_BIT_MASK (0x1 << 6)
#define LONGMODE_PAGING_TABLE_ENTRY_PAT_BIT_MASK (0x1 << 7)
#define LONGMODE_PAGING_TABLE_ENTRY_GLOBAL_BIT_MASK (0x1 << 8)
#define LONGMODE_PAGING_TABLE_ENTRY_ADDRESS_MASK (0xFFFFFFFFFF << 12)
#define LONGMODE_PAGING_TABLE_ENTRY_PROTECTION_KEY_MASK (0xF << 59)
#define LONGMODE_PAGING_TABLE_ENTRY_EXECUTE_DISABLE_BIT_MASK (0x1 << 63)

typedef uint64_t longmode_paging_level_4_entry;
typedef uint64_t longmode_paging_pointer_table_entry;
typedef uint64_t longmode_paging_directory_entry;
typedef uint64_t longmode_paging_table_entry;

typedef struct bootloader_longmode_page_tables {
    longmode_paging_level_4_entry level_4_entries[LONGMODE_PAGING_ENTRY_COUNT];
    longmode_paging_pointer_table_entry pointer_table_entries[LONGMODE_PAGING_ENTRY_COUNT];
    longmode_paging_directory_entry directory_entries[LONGMODE_PAGING_ENTRY_COUNT];
    longmode_paging_table_entry table_entries[LONGMODE_PAGING_ENTRY_COUNT];
} bootloader_longmode_page_tables;

enum bios_memory_type_enum {
    // These values are defined by the BIOS memory mapping; they
    // (perhaps obviously) describe the type/state of a given region.

    BIOS_MEMORY_REGION_USABLE = 1,
    BIOS_MEMORY_REGION_RESERVED = 2,
    BIOS_MEMORY_REGION_ACPI_RECLAIMABLE = 3,
    BIOS_MEMORY_REGION_ACPI_NVS = 4,
    BIOS_MEMORY_REGION_BAD = 5,

    // This value is used by ALVM to indicate that the
    // memory region is useless (e.g. too small for page mapping).

    ALVM_MEMORY_REGION_USELESS = 6,
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
    uint64_t byte_length;
    enum bios_memory_type_enum type;
    extended_attributes_def extended_attributes;
} bios_memory_map_node;

typedef struct physical_memory_region {
    uint8_t* base_address;
    uint64_t byte_length;
    physical_memory_region* previous;
    physical_memory_region* next;
} physical_memory_region;

typedef struct memory_page {
    uint64_t base_address;
    memory_page* previous;
    memory_page* next;
} memory_page;

typedef struct virtual_memory_region {
    uint64_t virtual_base_address;
    uint64_t size_bytes;
    virtual_memory_region* previous;
    virtual_memory_region* next;
} virtual_memory_region;

// Until we have functional dynamic memory allocation, we'll
// have to allocate some memory here for these initial structs/pointers.

longmode_paging_level_4_entry initial_page_l4_entry;
longmode_paging_pointer_table_entry initial_page_pointer_table_entry;
longmode_paging_directory_entry initial_page_directory_entry;
longmode_paging_table_entry initial_page_table_entry;

physical_memory_region* free_physical_memory_region_list_ptr;
physical_memory_region* used_physical_memory_region_list_ptr;

memory_page initial_page;

virtual_memory_region initial_virtual_memory = {
    .virtual_base_address = VIRTUAL_MEMORY_START,
    .size_bytes = (UINT64_MAX - 1),
    .previous = NULL_POINTER,
    .next = NULL_POINTER,
};

virtual_memory_region bootstrap_virtual_memory;

virtual_memory_region* allocated_virtual_memory_list_ptr;
virtual_memory_region* free_virtual_memory_list_ptr;

bootloader_longmode_page_tables* bootloader_longmode_page_tables_ptr;

// Iterate over the BIOS mapped regions and look for one large enough to hold 1 page.
// Use that page for the initial page of virtual memory.

void mem_mgmt_init(
    uint16_t page_mapping_offset,
    uint16_t memory_map_offset,
    uint16_t memory_map_size
) {
    bootloader_longmode_page_tables_ptr = (bootloader_longmode_page_tables*) page_mapping_offset;

    mem_mgmt_init_parse_memory_map((bios_memory_map_node*) memory_map_offset, memory_map_size);
}

/**
 * @todo Handle the edge case where regions may overlap.
 */
void mem_mgmt_init_parse_memory_map(bios_memory_map_node* memory_map_ptr, uint16_t memory_map_size)
{
    for (uint16_t i = 0; i < memory_map_size; i++) {
        mem_mgmt_init_vet_region(memory_map_ptr);

        if (memory_map_ptr->type == BIOS_MEMORY_REGION_USABLE) {
            mem_mgmt_init_handle_usable_region(memory_map_ptr);
        }

        ++memory_map_ptr;
    }
}

void mem_mgmt_init_vet_region(bios_memory_map_node* memory_region_ptr)
{
    // (We can skip regions that aren't usable in the first place.)

    if (memory_region_ptr->type != BIOS_MEMORY_REGION_USABLE) {
        return;
    }

    // Find how many bytes the base address is away from page alignment.
    // If it's more than 0, we need to ensure the base address is page-aligned
    // & that the region length is updated to reflect the new page-aligned region size.

    uint64_t offset_bytes = (memory_region_ptr->base_address % MEMORY_PAGE_SIZE_BYTES);

    if (offset_bytes > 0) {
        uint64_t shift_bytes = (MEMORY_PAGE_SIZE_BYTES - offset_bytes);

        memory_region_ptr->base_address += shift_bytes;
        memory_region_ptr->byte_length -= shift_bytes;
    }

    // If the final length isn't enough for a full page, the region is useless to us.

    if (memory_region_ptr->byte_length < MEMORY_PAGE_SIZE_BYTES) {
        memory_region_ptr->type = ALVM_MEMORY_REGION_USELESS;
    }
}

void mem_mgmt_init_handle_usable_region(bios_memory_map_node* free_region_ptr)
{
    // If this is the first decent region we've found, we'll [...]
    // @todo

    if (root_memory_region_ptr == NULL_POINTER) {
        mem_mgmt_init_handle_first_region(free_region_ptr);
    } else {
        // mem_mgmt_init_handle_subsequent_region(free_region_ptr);
    }
}

void mem_mgmt_init_handle_first_region(bios_memory_map_node* free_region_ptr)
{
    // @todo break this into smaller functions.

    // mem_mgmt_init_setup_region(free_region_ptr, &root_memory_region);

    // Describe/map the first page.

    initial_page.base_address = free_region_ptr->base_address;
    initial_page.next = NULL_POINTER;
    initial_page.previous = NULL_POINTER;



    /* mem_mgmt_map_virtual_page(
        initial_virtual_memory.virtual_base_address,
        initial_page.base_address
    );*/

    // @todo update the page tables

    // Here we'll "allocate" the first page of virtual memory.

    bootstrap_virtual_memory.virtual_base_address = initial_virtual_memory.virtual_base_address;
    bootstrap_virtual_memory.size_bytes = MEMORY_PAGE_SIZE_BYTES;
    bootstrap_virtual_memory.next = NULL_POINTER;
    bootstrap_virtual_memory.previous = NULL_POINTER;

    allocated_virtual_memory_list_ptr = &bootstrap_virtual_memory;

    // "initial_virtual_memory" now needs to describe the remaining free virtual
    // memory space (i.e. the original virtual space minus what we just allocated).

    initial_virtual_memory.virtual_base_address += MEMORY_PAGE_SIZE_BYTES;
    initial_virtual_memory.size_bytes -= MEMORY_PAGE_SIZE_BYTES;

    free_virtual_memory_list_ptr = &initial_virtual_memory;

    // Here we'll map the virtual address to the physical page.

    uint16_t level_4_index = (bootstrap_virtual_memory.virtual_base_address >> 39) & 0x1FF;
    uint16_t pointer_table_index = (bootstrap_virtual_memory.virtual_base_address >> 30) & 0x1FF;
    uint16_t directory_index = (bootstrap_virtual_memory.virtual_base_address >> 21) & 0x1FF;
    uint16_t page_table_index = (bootstrap_virtual_memory.virtual_base_address >> 12) & 0x1FF;

    // If the free physical region we've used here contains more
    // memory than we've allocated, we need to define
    // two physical memory regions - one in use (because we just manually
    // allocated it) - and one that is still free for use.

    uint64_t remaining_free_bytes = (free_region_ptr->byte_length - MEMORY_PAGE_SIZE_BYTES);

    if (remaining_free_bytes > 0) {
        free_physical_memory_region_list_ptr = mem_mgmt_alloc(sizeof(physical_memory_region));

        free_physical_memory_region_list_ptr->base_address = (free_region_ptr->base_address + MEMORY_PAGE_SIZE_BYTES);
        free_physical_memory_region_list_ptr->byte_length = remaining_free_bytes;
        free_physical_memory_region_list_ptr->next = NULL_POINTER;
        free_physical_memory_region_list_ptr->previous = NULL_POINTER;
    }

    used_physical_memory_region_list_ptr = mem_mgmt_alloc(sizeof(physical_memory_region));

    used_physical_memory_region_list_ptr->base_address = free_region_ptr->base_address;
    used_physical_memory_region_list_ptr->byte_length = MEMORY_PAGE_SIZE_BYTES;
    used_physical_memory_region_list_ptr->next = NULL_POINTER;
    used_physical_memory_region_list_ptr->previous = NULL_POINTER;

    // By the time we get here, dynamic memory allocation will semi-work
    // well enough for us to continue to bootstrap it without relying on
    // pre-allocated data.

    // @tood Flush TLB?
}

void mem_mgmt_map_virtual_page(uint64_t virtual_address, uint64_t physical_address)
{
    // @todo (Add to page table)


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
    new_memory_region->total_page_count = (free_region_ptr->byte_length / MEMORY_PAGE_SIZE_BYTES);
    new_memory_region->free_page_count = new_memory_region->total_page_count;
    new_memory_region->next_region = NULL_POINTER;
}

/**
 * (Not to be confused with userspace malloc.)
 * @todo
 */
uint8_t* mem_mgmt_alloc(uint64_t requested_byte_length)
{
    // Find a virtual region that is suitable.
    // If the region isn't already page mapped, map it.
}

virtual_memory_region* mem_mgmt_find_suitable_virtual_memory(uint64_t requested_byte_length)
{
    virtual_memory_region* free_virtual_memory_region = next_free_virtual_memory_region;

    while (free_virtual_memory_region != NULL_POINTER) {
        if (free_virtual_memory_region->size_bytes >= requested_byte_length) {
            return mem_mgmt_alloc_virtual_memory(free_virtual_memory_region, requested_byte_length);
        }

        free_virtual_memory_region = free_virtual_memory_region->next;
    }

    return NULL_POINTER;
}

virtual_memory_region* mem_mgmt_alloc_virtual_memory(
    virtual_memory_region* free_virtual_memory_region,
    uint64_t requested_byte_length
) {


    // Here we'll split free_virtual_memory_region into two regions - one
    // of which will be requested_byte_length in size, etc.
}

/**
 * (Not to be confused with userspace free.)
 * @todo
 */
void mem_mgmt_free(uint8_t* pointer)
{

}

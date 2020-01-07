#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

struct atmega_memory_config {
    uint32_t signature;
    char * name;
    int flash_page_size_in_words;
    int flash_number_of_pages;
    int eeprom_page_size_in_bytes;
    int eeprom_number_of_pages;
    bool has_extended_fuse_bits;
};

const struct atmega_memory_config * get_atmega_memory_config(uint32_t signature_bits);

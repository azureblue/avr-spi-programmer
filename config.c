#include "config.h"

static struct atmega_memory_config available_configs[] = {
    {.signature = 0x1E9307, .name = "ATmega8A", 
        .flash_page_size_in_words = 32, .flash_number_of_pages = 128, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 128, .has_extended_fuse_bits = false},

    {.signature = 0x1E9205, .name = "ATmega48A", 
        .flash_page_size_in_words = 32, .flash_number_of_pages = 64, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 64, .has_extended_fuse_bits = true},

    {.signature = 0x1E920A, .name = "ATmega48PA", 
        .flash_page_size_in_words = 32, .flash_number_of_pages = 64, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 64, .has_extended_fuse_bits = true},

    {.signature = 0x1E930A, .name = "ATmega88A", 
        .flash_page_size_in_words = 32, .flash_number_of_pages = 128, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 128, .has_extended_fuse_bits = true},
        
    {.signature = 0x1E930F, .name = "ATmega88PA", 
        .flash_page_size_in_words = 32, .flash_number_of_pages = 128, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 128, .has_extended_fuse_bits = true},
    
    {.signature = 0x1E9406, .name = "ATmega168A", 
        .flash_page_size_in_words = 64, .flash_number_of_pages = 128, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 128, .has_extended_fuse_bits = true},
    
    {.signature = 0x1E940B, .name = "ATmega168PA", 
        .flash_page_size_in_words = 64, .flash_number_of_pages = 128, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 128, .has_extended_fuse_bits = true},

    {.signature = 0x1E9514, .name = "ATmega328", 
        .flash_page_size_in_words = 64, .flash_number_of_pages = 256, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 256, .has_extended_fuse_bits = true},
    
    {.signature = 0x1E950F, .name = "ATmega328P", 
        .flash_page_size_in_words = 64, .flash_number_of_pages = 256, .eeprom_page_size_in_bytes = 4, .eeprom_number_of_pages = 256, .has_extended_fuse_bits = true},
    
    {.signature = 0}
};

const struct atmega_memory_config * get_atmega_memory_config(uint32_t signature_bits) {
    signature_bits &= 0xFFFFFF;
    for(int i = 0; available_configs[i].signature != 0; i++)
        if (available_configs[i].signature == signature_bits)
            return &available_configs[i];
    
    return NULL;
}

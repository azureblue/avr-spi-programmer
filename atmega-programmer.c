#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/spi/spidev.h>

#include "gpio.h"
#include "config.h"

extern noreturn void print_error_end_exit(char * error);
extern void delay(int ms);

#define instruction_size 4
#define word_size 2

static uint32_t flash_size;
static uint32_t page_size_in_words;
static uint32_t page_size_in_bytes;
static uint32_t page_offset;

static const int OP_WRITE_FUSE_LOW = 1, OP_WRITE_FUSE_HIGH = 2, OP_WRITE_FUSE_EXTENDED = 4, OP_WRITE_FLASH = 8;

struct {
    uint32_t enabled_operations_bitset;
    uint8_t fuse_low;
    uint8_t fuse_high;
    uint8_t fuse_extended;
    const char * flash_file;
} op_params = {.enabled_operations_bitset = 0};

struct {
    int reset_gpio_pin;
    int spi_mode;
    int spi_speed;
    char * spi_device;
} conn_params = {
    .reset_gpio_pin = 22,
    .spi_mode = 0,
    .spi_speed = 200000,
    .spi_device = "/dev/spidev0.0"
};

static int spi_fd = -1;

static const struct atmega_memory_config * mem_config = NULL;

void setup(const struct atmega_memory_config * conf) {
    mem_config = conf;
    flash_size = conf->flash_page_size_in_words * conf->flash_number_of_pages * word_size;
    page_size_in_words = conf->flash_page_size_in_words;
    page_size_in_bytes = page_size_in_words * word_size;
    page_offset = 0;
    uint32_t ps = page_size_in_words;
    while (ps >>= 1 != 0)
        page_offset++;
}

void send_data(uint8_t * tx, uint8_t * rx, int len) {
    struct spi_ioc_transfer ioc_transfer =  {
        .tx_buf = (intptr_t) tx,
        .rx_buf = (intptr_t) rx,
        .len = len,
        .speed_hz = conn_params.spi_speed,
        .delay_usecs = 0,
        .bits_per_word = 8
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &ioc_transfer) == -1)
        print_error_end_exit("sending instruction failed");

#ifdef DEBUG
    for(int i = 0; i < len; i += 4) {
        uint8_t * buf_ptr = tx_buf + i;
        printf("%.2X %.2X %.2X %.2X : ", buf_ptr[0], buf_ptr[1], buf_ptr[2], buf_ptr[3]);
        buf_ptr = rx_buf + i;
        printf("%.2X %.2X %.2X %.2X", buf_ptr[0], buf_ptr[1], buf_ptr[2], buf_ptr[3]);
    }
    printf("\n");
#endif
}

static void set_instruction(uint8_t * buf, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    buf[0] = byte1;
    buf[1] = byte2;
    buf[2] = byte3;
    buf[3] = byte4;
}

uint32_t send_instruction(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    uint8_t tx[] = {byte1, byte2, byte3, byte4};
    uint8_t rx[] = {0x00, 0x00, 0x00, 0x00};
    send_data(tx, rx, 4);
    return rx[0] << 24 | rx[1] << 16 | rx[2] << 8 | rx[3];
}

uint8_t read_signature_byte(uint8_t addr) {
    return send_instruction(0x30, 0x00, addr, 0x00) & 0xFF;
}

uint8_t read_fuse_bits(bool high) {
    return send_instruction(0x50 | (high << 3), 0x00 | (high << 3), 0x00, 0x00) & 0xFF;
}

uint8_t read_fuse_bits_extended() {
    return send_instruction(0x50, 0x08, 0x00, 0x00) & 0xFF;
}

void enable_programming() {
    uint8_t echo = send_instruction(0xAC, 0x53, 0x00, 0x00) >> 8 & 0xFF;
    if (echo != 0x53)
        print_error_end_exit("unable to enter programming mode");
}

uint32_t read_signature() {
    uint8_t b1 = read_signature_byte(0x00);
    uint8_t b2 = read_signature_byte(0x01);
    uint8_t b3 = read_signature_byte(0x02);
    return b1 << 16 | b2 << 8 | b3;
}

void reset_short_pulse() {
    if (!gpio_set(conn_params.reset_gpio_pin, 1))
        print_error_end_exit("unable to set gpio pin value");
    
    delay(100);
    
    if (!gpio_set(conn_params.reset_gpio_pin, 0))
        print_error_end_exit("unable to set gpio pin value");
}

void show_progress(const char * message, const char * units, int count, int limit) {
    printf("\r");
    printf("%s \t%d/%d %s ", message, count, limit, units);
    if (count == limit)
        puts("");
    fflush(stdout);
}

void read_program_memory(uint16_t word_addres, uint8_t buffer[], int len, bool progress) {
    uint32_t buffer_size = page_size_in_bytes * 4;
    uint8_t tx_buf[buffer_size];
    uint8_t rx_buf[buffer_size];
    int bytes_to_read_in_each_message = page_size_in_bytes;

    bool high = false;
    for (int i = 0; i < len;) {
        int bytes_for_current_read = len - i;
        if (bytes_for_current_read > bytes_to_read_in_each_message)
            bytes_for_current_read = bytes_to_read_in_each_message;

        for (int j = 0; j < bytes_for_current_read; j++) {
            uint16_t addr = word_addres + (i + j) / 2;
            set_instruction(tx_buf + j * 4, 0x20 | (high << 3), addr >> 8, addr & 0xFF, 0x00);
            high = !high;
        }

        send_data(tx_buf, rx_buf, bytes_for_current_read * 4);

        for (int j = 0; j < bytes_for_current_read; j++)
            buffer[i + j] = rx_buf[j * 4 + 3];

        if (progress)
            show_progress(" # reading flash", "bytes", i + bytes_for_current_read, len);

        i += bytes_for_current_read;
    }
}

void erase_chip() {
    printf(" # performing chip erase\n");
    send_instruction(0xAC, 0x80, 0x00, 0x00);
    delay(10);
}

void write_program_memory_page(int page_number) {
    uint8_t hb = (page_number >> (8 - page_offset)) & 0xFF;
    uint8_t lb = (page_number << (page_offset)) & 0xFF;
    send_instruction(0x4C, hb, lb, 0x00);
    delay(10);
}

void write_fuse_bits(uint8_t value, bool high) {
    send_instruction(0xAC, 0xA0 | high << 3, 0, value);
    printf(" # writting fuse %s: 0x%.2X\n", high ? "high" : "low", value);
    delay(10);
}

void write_fuse_bits_extended(uint8_t value) {
    send_instruction(0xAC, 0xA4, 0, value);
    printf(" # writting fuse extended: 0x%.2X\n", value);
    delay(10);
}

void write_flash(uint8_t data[], int len, bool progress) {
    uint32_t buffer_size = page_size_in_bytes * 4;
    uint8_t tx_buf[buffer_size];

    for (int i = 0, page = 0; i < len;) {
        int bytes_for_current_page = len - i;
        if (bytes_for_current_page > page_size_in_bytes)
            bytes_for_current_page = page_size_in_bytes;

        bool high = false;
        for (int j = 0; j < bytes_for_current_page; j++) {
            set_instruction(tx_buf + j * 4, 0x40 | (high << 3), 0x00, j / 2, data[i + j]);
            high = !high;
        }
        if (progress)
            show_progress(" # writting flash", "bytes", i + bytes_for_current_page, len);

        send_data(tx_buf, NULL, bytes_for_current_page * 4);
        write_program_memory_page(page);
        page++;
        i += page_size_in_bytes;
    }
}

void write_flash_from_file(const char *path) {
    uint8_t file_data[flash_size];
    uint8_t flash_data[flash_size];
    struct stat file_stat;

    if (stat(path, &file_stat))
        print_error_end_exit("unable to open file");

    size_t file_len = file_stat.st_size;

    if (file_len > flash_size)
        print_error_end_exit("file is larger than flash size");

    FILE *fileptr = fopen(path, "rb");
    if (!fileptr)
        print_error_end_exit("unable to open file");

    for (int i = 0; !feof(fileptr);) {
        int r = fread(file_data + i, 1, 1024, fileptr);
        i += r;
    }

    erase_chip();
    write_flash(file_data, file_len, true);
    reset_short_pulse();
    enable_programming();
    read_program_memory(0, flash_data, file_len, true);

    bool mismatch = false;
    for (int i = 0; i < file_len; i++) {
        if (file_data[i] != flash_data[i]) {
            printf("data mismatch at byte %d: expected %.2X, got %.2X\n",
                   i, file_data[i], flash_data[i]);
            mismatch = true;
        }
    }
    if (mismatch)
        puts("flash programming faild: data mismatch");
    else
        puts("flash programming OK!");
}

void parse_cmd_line(int argc, char ** argv) {
    static struct option long_options[] = {
        {"fuseL", required_argument, NULL, 'l'},
        {"fuseH", required_argument, NULL, 'h'},
        {"fuseE", required_argument, NULL, 'e'},
        {"flash", required_argument, NULL, 'f'},
        {"spi-driver", required_argument, NULL, 'd'},
        {"spi-speed-hz", required_argument, NULL, 's'},
        {"reset-pin", required_argument, NULL, 'r'},
        {NULL, 0, NULL, 0}
    };
    int c;
    while ((c = getopt_long(argc, argv, "l:h:f:", long_options, NULL)) != -1) {
        switch (c) {
        case 'l':
            if (sscanf(optarg, "%hhx", &op_params.fuse_low) == 0)
                print_error_end_exit("unable to parse --fuseL argument");
            op_params.enabled_operations_bitset |= OP_WRITE_FUSE_LOW;
            break;
        case 'h':
            if (sscanf(optarg, "%hhx", &op_params.fuse_high) == 0)
                print_error_end_exit("unable to parse --fuseH argument");
            op_params.enabled_operations_bitset |= OP_WRITE_FUSE_HIGH;
            break;
        case 'e':
            if (sscanf(optarg, "%hhx", &op_params.fuse_extended) == 0)
                print_error_end_exit("unable to parse --fuseE argument");
            op_params.enabled_operations_bitset |= OP_WRITE_FUSE_EXTENDED;
            break;
        case 'f':
            op_params.flash_file = optarg;
            op_params.enabled_operations_bitset |= OP_WRITE_FLASH;
            break;

//

        case 'd':
            conn_params.spi_device = optarg;
            break;
        case 's':
            if (sscanf(optarg, "%d", &conn_params.spi_speed) == 0)
                print_error_end_exit("unable to parse --spi-speed-hz argument");
            break;
        case 'r':
            if (sscanf(optarg, "%d", &conn_params.reset_gpio_pin) == 0)
                print_error_end_exit("unable to parse --reset-pin argument");
            break;
        case '?':
            exit(-1);
            break;
        }
    }
}

int main(int argc, char ** argv) {
    parse_cmd_line(argc, argv);
    if (!gpio_init_out(conn_params.reset_gpio_pin)) {
        print_error_end_exit("unable to setup gpio pin");
    }

    spi_fd = open(conn_params.spi_device, O_RDWR);
    if (spi_fd == -1) {
        perror("cannot open spi device");
        exit(-1);
    }

    if (ioctl(spi_fd, SPI_IOC_RD_MODE, &conn_params.spi_mode) == -1)
        print_error_end_exit("setting spi read mode failed");
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &conn_params.spi_mode) == -1)
        print_error_end_exit("setting spi write mode failed");
    // for some reason SPI_IOC_WR_MAX_SPEED_HZ needs to be setup first...
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &conn_params.spi_speed) == -1)
        print_error_end_exit("setting spi write max speed failed");
    if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &conn_params.spi_speed) == -1)
        print_error_end_exit("setting spi read max speed failed");

    reset_short_pulse();

    enable_programming();

    uint32_t signature_bits = read_signature();
    printf("device signature: 0x%.2X 0x%.2X 0x%.2X", signature_bits >> 16 & 0xFF, signature_bits >> 8 & 0xFF, signature_bits & 0xFF);

    const struct atmega_memory_config * config = get_atmega_memory_config(signature_bits);
    if (config == NULL)
        print_error_end_exit(" # ATmega microcontroller not recognized (or not supported)!");

    printf(" # %s\n", config->name);

    setup(config);

    printf("fuses:");
    uint8_t hfuse = read_fuse_bits(true);
    printf("  h:0x%.2X", hfuse);
    uint8_t lfuse = read_fuse_bits(false);
    printf("  l:0x%.2X", lfuse);
    if (config->has_extended_fuse_bits) {
        uint8_t efuse = read_fuse_bits_extended();
        printf("  e:0x%.2X", efuse);
    }
    printf("\n");

    if (op_params.enabled_operations_bitset & OP_WRITE_FUSE_LOW)
        write_fuse_bits(op_params.fuse_low, false);

    if (op_params.enabled_operations_bitset & OP_WRITE_FUSE_HIGH)
        write_fuse_bits(op_params.fuse_high, true);

    if (op_params.enabled_operations_bitset & OP_WRITE_FUSE_EXTENDED) {
        if (!config->has_extended_fuse_bits) {
            print_error_end_exit("this microcontroller doesn't have extended fuse bits!");
        }
        write_fuse_bits_extended(op_params.fuse_extended);
    }

    if (op_params.enabled_operations_bitset & OP_WRITE_FLASH)
        write_flash_from_file(op_params.flash_file);

    reset_short_pulse();
    gpio_set(conn_params.reset_gpio_pin, 1);

    close(spi_fd);
    return 0;
}

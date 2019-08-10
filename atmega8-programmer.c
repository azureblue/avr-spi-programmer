#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/spi/spidev.h>

#include "gpio.h"
extern noreturn void print_error_end_exit(char * error);

#define flash_size (1024 * 8)
#define page_size_in_words 32
#define page_size_in_bytes 64

int gpioPin = 22;
int mode = 0;
int speed = 100000;
char * driver = "/dev/spidev0.0";

static int spi_fd = -1;

static uint8_t tx_buf[page_size_in_bytes * 4];
static uint8_t rx_buf[page_size_in_bytes * 4];

void delay(int ms) {
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000,
    };
    if (nanosleep(&req, NULL)) {
        print_error_end_exit("sleep error");
    }
}

void send_data(int len) {
    struct spi_ioc_transfer ioc_transfer =  {
        .tx_buf = (intptr_t) tx_buf,
        .rx_buf = (intptr_t) rx_buf,
        .len = len,
        .speed_hz = speed,
        .delay_usecs = 0,
        .bits_per_word = 8
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &ioc_transfer) == -1)
        print_error_end_exit("sending instruction failed");

#ifdef DEBUG
    for(int i = 0; i < len; i += 4) {
        uint8_t * buf_ptr = tx_buf + i;
        printf("%.2X %.2X %.2X %.2X : ", buf_ptr, buf_ptr + 1, buf_ptr + 2, buf_ptr + 3);
        buf_ptr = rx_buf + i;
        printf("%.2X %.2X %.2X %.2X", buf_ptr, buf_ptr + 1, buf_ptr + 2, buf_ptr + 3);
    }
    printf("\n");
#endif
}

static void set_instruction(int offset, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    tx_buf[offset + 0] = byte1;
    tx_buf[offset + 1] = byte2;
    tx_buf[offset + 2] = byte3;
    tx_buf[offset + 3] = byte4;
}

uint8_t read_signature_byte(uint8_t addr) {
    set_instruction(0, 0x30, 0x00, addr, 0x00);
    send_data(4);
    return rx_buf[3];
}

uint8_t read_fuse_bits(bool high) {
    set_instruction(0, 0x50 | (high << 3), 0x00, 0x00, 0x00);
    send_data(4);
    return rx_buf[3];
}

void enable_programming() {
    set_instruction(0, 0xAC, 0x53, 0x00, 0x00);
    send_data(4);
    if (rx_buf[2] != 0x53)
        print_error_end_exit("unable to enter programming mode");
}

void show_progress(const char * message, const char * units, int count, int limit) {
        printf("\r");
        printf("%s \t%d/%d %s ", message, count, limit, units);
        if (count == limit)
            puts("");
        fflush(stdout);            
}

void read_program_memory(uint16_t word_addres, uint8_t buffer[], int len, bool progress) {
    bool high = false;
    int bytes_to_read_in_each_message = page_size_in_bytes;

    for (int i = 0; i < len;) {
        int bytes_for_current_read = len - i;
        if (bytes_for_current_read > bytes_to_read_in_each_message)
            bytes_for_current_read = bytes_to_read_in_each_message;

        for (int j = 0; j < bytes_for_current_read; j++) {
            uint16_t addr = word_addres + (i + j) / 2;
            set_instruction(j * 4, 0x20 | (high << 3), addr >> 8, addr & 0xFF, 0x00);
            high = !high;
        }

        send_data(bytes_for_current_read * 4);

        for (int j = 0; j < bytes_for_current_read; j++)
            buffer[i + j] = rx_buf[j * 4 + 3];

        if (progress)
            show_progress(" # reading flash", "bytes", i + bytes_for_current_read, len);    

        i += bytes_for_current_read;
    }
}

void erase_chip() {
    printf(" # performing chip erase\n");
    set_instruction(0, 0xAC, 0x80, 0x00, 0x00);
    send_data(4);
    delay(10);
}

void write_program_memory_page(int page_number) {
    uint8_t hb = (page_number >> 3) & 0xFF;
    uint8_t lb = (page_number << 5) & 0xFF;
    set_instruction(0, 0x4C, hb, lb, 0x00);
    send_data(4);
    delay(10);
}

void write_flash(uint8_t data[], int len, bool progress) {
    for (int i = 0, page = 0; i < len;) {        
        int bytes_for_current_page = len - i;
        if (bytes_for_current_page > page_size_in_bytes)
            bytes_for_current_page = page_size_in_bytes;

        bool high = false;
        for (int j = 0; j < bytes_for_current_page; j++) {
            set_instruction(j * 4, 0x40 | (high << 3), 0x00, j / 2, data[i + j]);
            high = !high;
        }
        if (progress)
            show_progress(" # writting flash", "bytes", i + bytes_for_current_page, len);
        send_data(bytes_for_current_page * 4);
        write_program_memory_page(page);
        page++;
        i += page_size_in_bytes;
    }
}

void check_signature() {
    uint8_t b1 = read_signature_byte(0x00);
    uint8_t b2 = read_signature_byte(0x01);
    uint8_t b3 = read_signature_byte(0x02);

    printf("device signature: 0x%.2X 0x%.2X 0x%.2X", b1, b2, b3);
    if (b1 == 0x1E && b2 == 0x93 && b3 == 0x07)
        printf (" | ATmega8\n");
    else
        print_error_end_exit(" : not matching ATmega8!");
}

void write_flash_from_file(char *path) {
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

    gpio_set(gpioPin, 1);
    delay(100);
    gpio_set(gpioPin, 0);

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

int main(int argc, char ** argv) {    

    gpio_init_out(22);

    spi_fd = open(driver, O_RDWR);
    if (spi_fd == -1) {
        perror("cannot open spi driver");
        exit(-1);
    }

    if (ioctl(spi_fd, SPI_IOC_RD_MODE, &mode) == -1)
        print_error_end_exit("setting spi read mode failed");
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1)
        print_error_end_exit("setting spi write mode failed");
    // for some reason SPI_IOC_WR_MAX_SPEED_HZ needs to be first...
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1)
        print_error_end_exit("setting spi write max speed failed");
    if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) == -1)
        print_error_end_exit("setting spi read max speed failed");

    gpio_set(gpioPin, 1);
    delay(100);
    gpio_set(gpioPin, 0);

    enable_programming();
    check_signature();

    uint8_t lfuse = read_fuse_bits(false);
    uint8_t hfuse = read_fuse_bits(true);

    printf("fuses: h:0x%.2X  l:0x%.2X \n", hfuse, lfuse);

    if (argc > 1 && argv[1] != NULL)
        write_flash_from_file(argv[1]);

    gpio_set(gpioPin, 1);
    close(spi_fd);

    return 0;
}

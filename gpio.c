#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#define path_size_max 100

static bool check_file_exists(const char *path) {
    return (access(path, F_OK ) != -1);
}

static void concat_path(int gpio_pin, const char *gpio_file_name, char *out) {
    snprintf(out, path_size_max, "/sys/class/gpio/gpio%d/%s", gpio_pin, gpio_file_name);
}

static bool write_to_file(const char *path, const char *data) {
    FILE *file = fopen(path, "w");

    if (!file)
        return false;

    int ret = fprintf(file, "%s", data);

    fclose(file);
    return ret > 0;
}

static bool gpio_set_direction(int gpio_pin, const char * direction) {
    char path[path_size_max];
    concat_path(gpio_pin, "direction", path);
    return write_to_file(path, direction);
}

static bool gpio_export(int gpio_pin) {
    char gpio_pin_string[10];
    snprintf(gpio_pin_string, 10, "%d", gpio_pin);
    return write_to_file("/sys/class/gpio/export", gpio_pin_string);
}

bool gpio_init_out(int gpio_pin) {
    char path[path_size_max];
    concat_path(gpio_pin, "direction", path);
    if (!check_file_exists(path)) {
        bool gpio_export_success = gpio_export(gpio_pin);
        if (!gpio_export_success)
            return false;
    }

    if (!check_file_exists(path))
        return false;

    if (!gpio_set_direction(gpio_pin, "out"))
        return false;

    return true;
}

bool gpio_set(int gpio_pin, bool value) {
    char path[path_size_max];
    concat_path(gpio_pin, "value", path);
    return write_to_file(path, value ? "1" : "0");
}


#include "reader.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int copy_file(const char* path, int output) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) return 2;

    long capacity = 16;
    long used = 0;
    char* data = malloc(capacity);
    if (!data) {
        close(fd);
        return 3;
    }

    int result = 0;
    while (1) {
        if (used == capacity) {
            if (capacity > 4611686018427387903) {
                result = 3;
                break;
            }
            long next_capacity = capacity * 2;
            char* grown = realloc(data, next_capacity);
            if (!grown) {
                result = 3;
                break;
            }
            data = grown;
            capacity = next_capacity;
        }
        long count = read(fd, &data[used], capacity - used);
        if (count < 0) {
            result = 4;
            break;
        }
        if (count == 0) break;
        used += count;
    }

    if (close(fd) != 0 && result == 0) result = 5;
    if (result == 0) {
        long written = 0;
        while (written < used) {
            long count = write(output, &data[written], used - written);
            if (count <= 0) {
                result = 6;
                break;
            }
            written += count;
        }
    }
    free(data);
    return result;
}

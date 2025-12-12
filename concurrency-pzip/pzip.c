#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_LEN (512)
#define KB(x) ((size_t)x * 1024)
#define MB(x) (KB(x) * 1024)
#define GB(x) (MB(x) * 1024)

void
unix_error(char* const message, int err)
{
    fprintf(stderr, "%s: %s\n", message, strerror(err));
    exit(-1);
}

FILE*
Fopen(char* const name, char* const mode)
{
    FILE* file = fopen(name, "r");
    if (!file) {
        unix_error("fopen", errno);
    }
    return file;
}

int
Open(char* const name, const int mode)
{
    int fd = open(name, O_RDONLY);
    if (!fd) {
        unix_error("open", errno);
    }
    return fd;
}

void
Fstat(int fd, struct stat* filestat)
{
    if (fstat(fd, filestat) != 0) {
        unix_error("fstat", errno);
    }
}

void*
Mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset)
{
    char* data = mmap(addr, size, prot, flags, fd, offset);
    if (data == MAP_FAILED) {
        unix_error("mmap", errno);
    }
    return data;
}

void*
Calloc(size_t num, size_t size)
{
    char* ret = calloc(num, size);
    if (!ret) {
        unix_error("calloc", errno);
    }
    return ret;
}
void*
Malloc(size_t size)
{
    char* ret = malloc(size);
    if (!ret) {
        unix_error("malloc", errno);
    }
    return ret;
}

struct WorkTask
{
    size_t len;
    char out[];
};

struct WorkTask*
zip(char* buf, size_t buf_len)
{
    struct WorkTask* task = Malloc(sizeof(*task) + 5 * buf_len);
    char* out = task->out;
    size_t cursor = 0;
    size_t count = 0;
    char current = '\0';
    for (size_t i = 0; i < buf_len; i++) {
        char c = buf[i];
        if (c != current) {
            if (count > 0) {
                *(int*)(out + cursor) = count;
                cursor += 4;
                out[cursor++] = current;
            }
            current = c;
            count = 1;
        } else {
            count++;
        }
    }
    if (count > 0) {
        *(int*)(out + cursor) = count;
        cursor += 4;
        out[cursor++] = current;
    }

    task->len = cursor;
    return task;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        puts("pzip: file1 [file2 ...]");
        return EXIT_FAILURE;
    }

    struct WorkTask* tasks[argc - 1];
    for (size_t i = 1; i < argc; i++) {
        int fd = Open(argv[1], O_RDONLY);
        struct stat filestat;
        Fstat(fd, &filestat);
        char* buf = Mmap(NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);
        tasks[i - 1] = zip(buf, filestat.st_size);
        close(fd);
    }
    size_t i = 0;
    for (; i < argc - 2; i++) {
        struct WorkTask* task1 = tasks[i];
        struct WorkTask* task2 = tasks[i + 1];
        if (task1->out[task1->len - 1] == task2->out[4]) {
            task1->len -= 5;
            int run1 = *(int*)(task1->out + task1->len);
            int run2 = *(int*)(task2->out);
            *(int*)(task2->out) = run1 + run2;
        }
        fwrite(task1->out, task1->len, 1, stdout);
    }
    fwrite(tasks[i]->out, tasks[i]->len, 1, stdout);
    return EXIT_SUCCESS;
}

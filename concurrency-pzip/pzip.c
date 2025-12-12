#include <assert.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

void
Pthread_create(pthread_t* thread,
               pthread_attr_t* attr,
               void*(fn)(void*),
               void* arg)
{
    int err = pthread_create(thread, attr, fn, arg);
    if (err) {
        unix_error("pthread_create", err);
    }
}

void
Pthread_join(pthread_t p, void** ret)
{
    int err = pthread_join(p, ret);
    if (err) {
        unix_error("pthread_join", err);
    }
}

struct WorkTask
{
    size_t len;
    char* in;
};
struct WorkResult
{
    size_t len;
    char out[];
};

struct WorkResult*
zip(struct WorkTask* request)
{
    size_t buf_len = request->len;
    char* buf = request->in;
    struct WorkResult* task = Malloc(sizeof(*task) + 5 * buf_len);
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
void*
zip_thread(void* arg)
{
    return (void*)zip((struct WorkTask*)arg);
}

#define THREADS (16)
int
main(int argc, char* argv[])
{
    if (argc < 2) {
        puts("pzip: file1 [file2 ...]");
        return EXIT_FAILURE;
    }

    size_t task_len = (argc - 1) * THREADS;
    struct WorkResult* tasks[task_len];
    pthread_t threads[THREADS];
    for (size_t i = 1; i < argc; i++) {
        int fd = Open(argv[1], O_RDONLY);
        struct stat filestat;
        Fstat(fd, &filestat);
        char* buf = Mmap(NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);

        size_t per_thread = filestat.st_size / THREADS;
        size_t mod = filestat.st_size % THREADS;
        for (size_t j = 0; j < mod; j++) {
            struct WorkTask* task = Malloc(sizeof(*task));
            task->len = per_thread + 1;
            task->in = buf + (per_thread + 1) * j;
            Pthread_create(&threads[j], NULL, zip_thread, task);
        }
        size_t covered = (per_thread + 1) * mod;
        for (size_t j = 0; j < THREADS - mod; j++) {
            struct WorkTask* task = Malloc(sizeof(*task));
            task->len = per_thread;
            task->in = buf + covered + per_thread * j;
            Pthread_create(&threads[j + mod], NULL, zip_thread, task);
        }
        for (size_t j = 0; j < THREADS; j++) {
            Pthread_join(threads[j], (void*)&tasks[(i - 1) * THREADS + j]);
        }
        close(fd);
    }
    size_t i = 0;
    for (; i < task_len - 1; i++) {
        struct WorkResult* task1 = tasks[i];
        struct WorkResult* task2 = tasks[i + 1];
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

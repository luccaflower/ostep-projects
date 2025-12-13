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
#define THREADS (16)

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
    if (fd < 0) {
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

void*
Realloc(void* ptr, size_t newsize)
{
    char* ret = realloc(ptr, newsize);
    if (!ret) {
        unix_error("realloc", errno);
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

void
Pthread_mutex_lock(pthread_mutex_t* lock)
{
    int err = pthread_mutex_lock(lock);
    if (err) {
        unix_error("pthread_mutex_lock", err);
    }
}
void
Pthread_mutex_unlock(pthread_mutex_t* lock)
{
    int err = pthread_mutex_unlock(lock);
    if (err) {
        unix_error("pthread_mutex_unlock", err);
    }
}

void
Pthread_cond_init(pthread_cond_t* cond, pthread_condattr_t* attr)
{
    int err = pthread_cond_init(cond, attr);
    if (err) {
        unix_error("pthread_cond_init", err);
    }
}

void
Pthread_cond_signal(pthread_cond_t* cond)
{
    int err = pthread_cond_signal(cond);
    if (err) {
        unix_error("pthread_cond_signal", err);
    }
}

void
Pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* lock)
{
    int err = pthread_cond_wait(cond, lock);
    if (err) {
        unix_error("pthread_cond_wait", err);
    }
}

enum WorkStatus
{
    NOT_STARTED,
    WAITING_FOR_MERGE,
    MERGED,
    PRINTED
};
struct WorkResult
{
    enum WorkStatus status;
    size_t len;
    char out[];
};
struct WorkResultList
{
    size_t capacity;
    size_t len;
    struct WorkResult* list[];
};

struct WorkResultList*
List_new(size_t initial_capacity)
{
    struct WorkResultList* list =
      Malloc(sizeof(*list) + initial_capacity * sizeof(*list->list));
    list->len = 0;
    list->capacity = initial_capacity;
    return list;
}

void
List_add(struct WorkResult* result, struct WorkResultList** list)
{
    if ((*list)->capacity >= (*list)->len) {
        size_t new_capacity = (*list)->capacity * 2;
        *list = Realloc(*list,
                        sizeof(**list) + new_capacity * sizeof(*(*list)->list));
        (*list)->capacity = new_capacity;
    }
    (*list)->list[(*list)->len++] = result;
}

struct WorkTask
{
    size_t len;
    char* in;
    struct WorkResult* result;
};

#define MAX (8)
struct WorkContext
{
    pthread_cond_t empty, fill, done;
    pthread_mutex_t lock;
    size_t produceptr;
    size_t consumeptr;
    size_t waiting_count;
    size_t done_count;
    size_t all_tasks;
    struct WorkTask* buffer[MAX];
};

void
init_work_context(struct WorkContext* context)
{
    context->produceptr = 0;
    context->consumeptr = 0;
    context->waiting_count = 0;
    context->all_tasks = 0;
    pthread_mutex_init(&context->lock, NULL);
    Pthread_cond_init(&context->empty, NULL);
    Pthread_cond_init(&context->fill, NULL);
    Pthread_cond_init(&context->done, NULL);
}

void
put_task(struct WorkContext* context, struct WorkTask* task)
{
    Pthread_mutex_lock(&context->lock);
    while (context->waiting_count == MAX) {
        Pthread_cond_wait(&context->empty, &context->lock);
    }
    context->buffer[context->produceptr] = task;
    context->produceptr = (context->produceptr + 1) % MAX;
    context->waiting_count++;
    context->all_tasks++;
    Pthread_cond_signal(&context->fill);
    Pthread_mutex_unlock(&context->lock);
}

struct WorkTask*
consume_task(struct WorkContext* context)
{
    Pthread_mutex_lock(&context->lock);
    while (context->waiting_count == 0) {
        Pthread_cond_wait(&context->fill, &context->lock);
    }
    struct WorkTask* task = context->buffer[context->consumeptr];
    context->consumeptr = (context->consumeptr + 1) % MAX;
    context->waiting_count--;
    Pthread_cond_signal(&context->empty);
    Pthread_mutex_unlock(&context->lock);
    return task;
}

void
zip(struct WorkTask* request)
{
    size_t buf_len = request->len;
    char* buf = request->in;
    struct WorkResult* result = request->result;
    char* out = result->out;
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

    result->len = cursor;
    result->status = WAITING_FOR_MERGE;
}
void*
zip_thread(void* arg)
{
    zip((struct WorkTask*)arg);
    return NULL;
}

void*
zip_consumer(void* arg)
{
    struct WorkContext* context = (struct WorkContext*)arg;
    while (1) {
        struct WorkTask* task = consume_task(context);
        zip(task);
        Pthread_mutex_lock(&context->lock);
        context->done_count++;
        if (context->done_count == context->all_tasks) {
            Pthread_cond_signal(&context->done);
        }
        Pthread_mutex_unlock(&context->lock);
    }
}

void
merge(struct WorkResult* task1, struct WorkResult* task2)
{
    task1->len -= 5;
    int run1 = *(int*)(task1->out + task1->len);
    int run2 = *(int*)(task2->out);
    *(int*)(task2->out) = run1 + run2;
    task1->status = MERGED;
}

#define CHUNK_SIZE (MB(64))
int
main(int argc, char* argv[])
{
    if (argc < 2) {
        puts("pzip: file1 [file2 ...]");
        return EXIT_FAILURE;
    }
    size_t task_len = (argc - 1) * THREADS;
    struct WorkContext* context = Malloc(sizeof(*context));
    init_work_context(context);

    struct WorkResultList* result_list = List_new(32);
    pthread_t consumers[THREADS];
    for (size_t i = 0; i < THREADS; i++) {
        Pthread_create(&consumers[i], NULL, zip_consumer, context);
    }
    for (size_t i = 1; i < argc; i++) {
        int fd = Open(argv[1], O_RDONLY);
        struct stat filestat;
        Fstat(fd, &filestat);
        char* buf = Mmap(NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);
        size_t even_chunks = filestat.st_size / CHUNK_SIZE;
        size_t final_chunk = filestat.st_size % CHUNK_SIZE;
        size_t total_chunks = even_chunks + (final_chunk == 0 ? 0 : 1);
        for (size_t j = 0; j < even_chunks; j++) {
            struct WorkTask* task = Malloc(sizeof(*task));
            task->len = CHUNK_SIZE;
            task->in = buf + CHUNK_SIZE * j;
            struct WorkResult* result =
              Malloc(sizeof(*result) + 5 * CHUNK_SIZE);
            result->status = NOT_STARTED;
            task->result = result;
            List_add(result, &result_list);
            put_task(context, task);
        }
        if (final_chunk) {
            struct WorkTask* task = Malloc(sizeof(*task));
            task->len = final_chunk;
            task->in = buf + CHUNK_SIZE * even_chunks;
            struct WorkResult* result =
              Malloc(sizeof(*result) + 5 * final_chunk);
            result->status = NOT_STARTED;
            task->result = result;
            List_add(result, &result_list);
            put_task(context, task);
        }

        close(fd);
    }
    Pthread_mutex_lock(&context->lock);
    while (context->done_count < context->all_tasks) {
        Pthread_cond_wait(&context->done, &context->lock);
    }
    Pthread_mutex_unlock(&context->lock);
    size_t i = 0;
    for (; i < result_list->len - 1; i++) {
        struct WorkResult* task1 = result_list->list[i];
        struct WorkResult* task2 = result_list->list[i + 1];
        if (task1->status == WAITING_FOR_MERGE &&
            task2->status == WAITING_FOR_MERGE &&
            task1->out[task1->len - 1] == task2->out[4]) {
            merge(task1, task2);
        }
        fwrite(task1->out, task1->len, 1, stdout);
        task1->status = PRINTED;
    }
    struct WorkResult* last = result_list->list[i];
    fwrite(last->out, last->len, 1, stdout);
    return EXIT_SUCCESS;
}

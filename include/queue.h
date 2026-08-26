//
// Created by Kok on 8/25/26.
//

#ifndef C_HTTP_SERVER_QUEUE_H
#define C_HTTP_SERVER_QUEUE_H

#include <pthread.h>

#define QUEUE_CAPACITY          128

typedef struct {
    void *ptrs[QUEUE_CAPACITY];        // structure pointers
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} queue_t;

/**
 * @brief Pushes structure pointers to the queue
 * @param queue Queue structure
 * @param fd Socket file descriptor
 */
void queue_push(void *ptr, queue_t *queue);

/**
 * @brief Gets the first structure pointer in the queue
 * @param queue Queue structure
 * @return Socket file descriptor
 */
void *queue_pop(queue_t *queue);

#endif //C_HTTP_SERVER_QUEUE_H

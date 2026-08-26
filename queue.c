//
// Created by Kok on 8/25/26.
//

#include "queue.h"

void queue_push(void *ptr, queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    queue->ptrs[queue->tail] = ptr;
    queue->tail = (queue->tail + 1) % QUEUE_CAPACITY;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

void *queue_pop(queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    void *ptr = queue->ptrs[queue->head];
    queue->head = (queue->head + 1) % QUEUE_CAPACITY;
    queue->count--;

    pthread_mutex_unlock(&queue->lock);
    return ptr;
}

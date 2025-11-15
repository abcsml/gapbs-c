#ifndef SLIDING_QUEUE_H_
#define SLIDING_QUEUE_H_

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "platform_atomics.h"

typedef struct {
  NodeID *shared;
  size_t capacity;
  size_t shared_in;
  size_t shared_out_start;
  size_t shared_out_end;
} SlidingQueue;

static inline void SlidingQueueInit(SlidingQueue *queue, size_t capacity) {
  queue->shared = (NodeID *)malloc(sizeof(NodeID) * capacity);
  queue->capacity = capacity;
  queue->shared_in = 0;
  queue->shared_out_start = 0;
  queue->shared_out_end = 0;
}

static inline void SlidingQueueFree(SlidingQueue *queue) {
  free(queue->shared);
  queue->shared = NULL;
  queue->capacity = 0;
  queue->shared_in = 0;
  queue->shared_out_start = 0;
  queue->shared_out_end = 0;
}

static inline void SlidingQueuePushBack(SlidingQueue *queue, NodeID value) {
  if (queue->shared_in >= queue->capacity) {
    size_t new_capacity = queue->capacity == 0 ? 1 : queue->capacity * 2;
    NodeID *new_shared = (NodeID *)realloc(queue->shared, sizeof(NodeID) * new_capacity);
    if (new_shared == NULL)
      abort();
    queue->shared = new_shared;
    queue->capacity = new_capacity;
  }
  queue->shared[queue->shared_in++] = value;
}

static inline int SlidingQueueEmpty(const SlidingQueue *queue) {
  return queue->shared_out_start == queue->shared_out_end;
}

static inline void SlidingQueueReset(SlidingQueue *queue) {
  queue->shared_out_start = 0;
  queue->shared_out_end = 0;
  queue->shared_in = 0;
}

static inline void SlidingQueueSlideWindow(SlidingQueue *queue) {
  queue->shared_out_start = queue->shared_out_end;
  queue->shared_out_end = queue->shared_in;
}

static inline size_t SlidingQueueCurrentBeginOffset(const SlidingQueue *queue) {
  return queue->shared_out_start;
}

static inline size_t SlidingQueueCurrentEndOffset(const SlidingQueue *queue) {
  return queue->shared_out_end;
}

static inline NodeID *SlidingQueuePointerAt(const SlidingQueue *queue, size_t offset) {
  return queue->shared + offset;
}

static inline NodeID *SlidingQueueBegin(const SlidingQueue *queue) {
  return queue->shared + queue->shared_out_start;
}

static inline NodeID *SlidingQueueEnd(const SlidingQueue *queue) {
  return queue->shared + queue->shared_out_end;
}

static inline size_t SlidingQueueSize(const SlidingQueue *queue) {
  return queue->shared_out_end - queue->shared_out_start;
}

typedef struct {
  size_t in;
  size_t local_size;
  NodeID *local_queue;
  SlidingQueue *master;
} QueueBuffer;

static inline void QueueBufferInit(QueueBuffer *buffer, SlidingQueue *queue, size_t local_size) {
  buffer->in = 0;
  buffer->local_size = local_size == 0 ? 16384 : local_size;
  buffer->local_queue = (NodeID *)malloc(sizeof(NodeID) * buffer->local_size);
  buffer->master = queue;
}

static inline void QueueBufferFree(QueueBuffer *buffer) {
  free(buffer->local_queue);
  buffer->local_queue = NULL;
  buffer->in = 0;
  buffer->local_size = 0;
  buffer->master = NULL;
}

static inline void QueueBufferFlush(QueueBuffer *buffer) {
  if (buffer->in == 0)
    return;
  SlidingQueue *queue = buffer->master;
  size_t copy_start = fetch_and_add_size(&queue->shared_in, buffer->in);
  if (copy_start + buffer->in > queue->capacity) {
    size_t new_capacity = queue->capacity == 0 ? 1 : queue->capacity;
    while (copy_start + buffer->in > new_capacity)
      new_capacity *= 2;
    NodeID *new_shared = (NodeID *)realloc(queue->shared, sizeof(NodeID) * new_capacity);
    if (new_shared == NULL)
      abort();
    queue->shared = new_shared;
    queue->capacity = new_capacity;
  }
  memcpy(queue->shared + copy_start, buffer->local_queue, buffer->in * sizeof(NodeID));
  buffer->in = 0;
}

static inline void QueueBufferPushBack(QueueBuffer *buffer, NodeID value) {
  if (buffer->in == buffer->local_size)
    QueueBufferFlush(buffer);
  buffer->local_queue[buffer->in++] = value;
}

#endif  // SLIDING_QUEUE_H_

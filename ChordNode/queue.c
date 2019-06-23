#include "queue.h"

int initqueue(Queue *q, int size) {
	if (size <= 0) return -1;
	q->capacity = size;
	q->front = q->rear = q->count = 0;
	q->data = (DATA_TYPE*)malloc(sizeof(DATA_TYPE) * q->capacity);
	if (q->data == NULL) return -1;
	return 0;
}
int destroyqueue(Queue *q) {
	free(q->data); q->data = NULL;
	q->capacity = q->front = q->rear = q->count = 0;
	return 0;
}
int queue_isFull(Queue *q) {
	if (q->count == q->capacity)
		return TRUE;
	return FALSE;
}
int queue_isEmpty(Queue *q) {
	if (q->count == 0)
		return TRUE;
	return FALSE;
}
int dequeue(Queue *q, DATA_PTYPE out_data) {
	if (queue_isEmpty(q))
		return -1;
	q->count--;
	*out_data = q->data[q->front];
	q->front = (q->front + 1) % q->capacity;
	return 0;
}
int enqueue(Queue *q, DATA_TYPE data) {
	if (queue_isFull(q))
		return -1;
	q->count++;
	q->data[q->rear] = data;
	q->rear = (q->rear + 1) % q->capacity;
	return 0;
}
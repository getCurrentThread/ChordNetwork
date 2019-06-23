#pragma once
#include "chordMsg.h"

#define DATA_TYPE RecvMsgDataType
#define DATA_PTYPE DATA_TYPE*
static const RecvMsgDataType DATA_END_VALUE = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, NULL };

typedef struct queue {
	DATA_PTYPE data;
	int capacity;
	int front, rear;//출력, 입력 인덱스
	int count;
}Queue;

int initqueue(Queue *q, int size);
int destroyqueue(Queue *q);
int queue_isFull(Queue *q);
int queue_isEmpty(Queue *q);
int dequeue(Queue *q, DATA_PTYPE out_data);
int enqueue(Queue *q, DATA_TYPE data);
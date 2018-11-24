/* Circular buffer example, keeps one slot open */
#include <stdlib.h>
#include <stdio.h>
#include "circbuf.h"
#include "ads1282.h"



/* Инициализация буфера, если его нет!  */
#pragma section("L1_code")
int cb_init(CircularBuffer * cb, int size)
{
    cb->size = size + 1;	/* include empty elem */
    cb->start = 0;
    cb->end = 0;

    if (cb->elems == NULL) {
	cb->elems = (ElemType *) calloc(cb->size, sizeof(ElemType));

	if (cb->elems == NULL)	/* Не смог выделить память */
	    return -1;
    }
   return 0;
}


/* Удалить буфер, если он существует! */
#pragma section("L1_code")
void cb_free(CircularBuffer * cb)
{
    if (cb->elems != NULL) {
	free(cb->elems);	/* OK if null */
	cb->elems = 0;
    }
}

/**
 * Буфер полный?
 */
#pragma section("L1_code")
int cb_is_full(CircularBuffer * cb)
{
    return (cb->end + 1) % cb->size == cb->start;
}

/**
 * Буфер пустой?
 */
#pragma section("L1_code")
int cb_is_empty(CircularBuffer * cb)
{
    return cb->end == cb->start;
}

/**
 * Очистить буфер
 */
#pragma section("L1_code")
void cb_clear(CircularBuffer * cb)
{
    cb->start = 0;
    cb->end = 0;    
}


/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
#pragma section("L1_code")
void cb_write(CircularBuffer * cb, ElemType * elem)
{
    cb->elems[cb->end] = *elem;
    cb->end = (cb->end + 1) % cb->size;
    if (cb->end == cb->start)
	cb->start = (cb->start + 1) % cb->size;	/* full, overwrite */
}

/* Read oldest element. App must ensure !cbIsEmpty() first. */
#pragma section("L1_code")
void cb_read(CircularBuffer * cb, ElemType * elem)
{
    *elem = cb->elems[cb->start];
     cb->start = (cb->start + 1) % cb->size;
}

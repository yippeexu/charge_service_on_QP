#include <system.h>
#include "fifo.h"

// fifo init 
fifo_t *fifo_init(fifo_t *fifo, uint8_t * buffer, uint32_t size)
{
	fifo->buffer = buffer;
	fifo->size = size;
	fifo->in = 0;
	fifo->out = 0;
	return fifo;
}

void fifo_reset(fifo_t *fifo)
{
	memset(fifo->buffer, 0, fifo->size);
	fifo->in = 0;
	fifo->out = 0;
}

uint32_t fifo_len(fifo_t *fifo)
{
	return fifo->in - fifo->out;
}

uint32_t fifo_in_c(fifo_t *fifo, uint8_t c)
{
	if (fifo->size - (fifo->in - fifo->out) == 0)
	{
		return 1;
	}
	fifo->buffer[fifo->in &(fifo->size - 1)] = c;
	fifo->in += 1;
	return 0;
}

uint32_t fifo_out_c(fifo_t *fifo, uint8_t *c)
{
	if (fifo->in - fifo->out == 0)
	{
		return 1;
	}
	*c = fifo->buffer[fifo->out &(fifo->size - 1)];
	fifo->out += 1;
	return 0;
}

#ifndef __FIFO_H__
#define __FIFO_H__ 


typedef struct {
	uint8_t *buffer;    /* the buffer holding the data */
	uint32_t size;    /* the size of the allocated buffer */
	uint32_t in;    /* data is added at offset (in % size) */
	uint32_t out;    /* data is extracted from off. (out % size) */
}fifo_t;

fifo_t *fifo_init(fifo_t *fifo, uint8_t * buffer, uint32_t size);
uint32_t fifo_in_c(fifo_t *fifo, uint8_t c);
uint32_t fifo_out_c(fifo_t *fifo, uint8_t *c);
uint32_t fifo_len(fifo_t *fifo);

#endif

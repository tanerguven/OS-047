#ifndef TYPES_H
#define TYPES_H

typedef unsigned int	uint32_t;
typedef          int	int32_t;
typedef unsigned short	uint16_t;
typedef          short	int16_t;
typedef unsigned char	uint8_t;
typedef          char	int8_t;

typedef char*			ptr_t;
typedef uint32_t		addr_t;

typedef struct  {
	addr_t a;
} __attribute__((packed)) va_t;

// typedef addr_t va_t;

typedef unsigned int	size_t;

#ifndef NULL
# define NULL 0
#endif

#endif /* TYPES_H */

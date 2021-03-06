#ifndef MCMAP_PROTOCOL_H
#define MCMAP_PROTOCOL_H 1

#include "types.h"
#include "platform.h"

/* protocol mess */

#include "protocol-data.h"

#define PACKET_TO_CLIENT 0x01
#define PACKET_TO_SERVER 0x02

#define PACKET_FLAG_IGNORE 0x04

enum field_type
{
	FIELD_BYTE,
	FIELD_SHORT,
	FIELD_INT,
	FIELD_LONG,
	FIELD_FLOAT,
	FIELD_DOUBLE,
	FIELD_STRING,
	FIELD_ITEM,
	FIELD_BYTE_ARRAY,
	FIELD_BLOCK_ARRAY,
	FIELD_ITEM_ARRAY,
	FIELD_EXPLOSION_ARRAY,
	FIELD_ENTITY_DATA
};

struct packet
{
	unsigned flags;
	unsigned id;
	unsigned size;
	unsigned char *bytes;
	unsigned *field_offset;
};

typedef struct packet packet_t;

#define MAX_PACKET_SIZE 262144
#define MAX_FIELDS 16

struct packet_state
{
	unsigned char buf[MAX_PACKET_SIZE];
	unsigned buf_start, buf_pos, buf_end;
	unsigned offset[MAX_FIELDS];
	struct packet p;
};

typedef struct packet_state packet_state_t;

#define PACKET_STATE_INIT(f) { .buf_start = 0, .buf_pos = 0, .buf_end = 0, .p = { .flags = f } }

packet_t *packet_read(socket_t sock, packet_state_t *state);

int packet_write(socket_t sock, packet_t *packet);

packet_t *packet_dup(packet_t *packet);

packet_t *packet_new(unsigned flags, enum packet_id type, ...);

void packet_free(gpointer packet);

int packet_nfields(packet_t *packet);

jint packet_int(packet_t *packet, unsigned field);
jlong packet_long(packet_t *packet, unsigned field);
double packet_double(packet_t *packet, unsigned field);
unsigned char *packet_string(packet_t *packet, unsigned field, int *len);

#endif /* MCMAP_PROTOCOL_H */

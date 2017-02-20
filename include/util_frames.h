#ifndef util_frames_h
#define util_frames_h

#include <stdint.h>
#include "lob.h"


typedef struct util_frames_s util_frames_s, *util_frames_t;

util_frames_t util_frames_new(uint32_t max, uint32_t magic);

util_frames_t util_frames_free(util_frames_t frames);

// turn this packet into frames and append, free's out
util_frames_t util_frames_send(util_frames_t frames, lob_t out);

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames);

// total bytes in the inbox/outbox
uint32_t util_frames_inlen(util_frames_t frames);
uint32_t util_frames_outlen(util_frames_t frames);

uint32_t util_frames_awaiting(util_frames_t frames);
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data, uint32_t len);

uint8_t *util_frames_outbox(util_frames_t frames, uint32_t *len);

util_frames_t util_frames_sent(util_frames_t frames);

// are we active to sending/receiving frames 
util_frames_t util_frames_busy(util_frames_t frames);

#endif

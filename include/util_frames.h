#ifndef util_frames_h
#define util_frames_h

#include <stdint.h>
#include "lob.h"


typedef struct util_frames_s util_frames_s, *util_frames_t;


// size of each frame, ideal is for(i=0;i<=1024;i++) if((i-4)%5 == 0 && (i%4)==0) console.log(i);
util_frames_t util_frames_new(uint16_t size, uint32_t ours, uint32_t theirs);

util_frames_t util_frames_free(util_frames_t frames);

// turn this packet into frames and append, free's out
util_frames_t util_frames_send(util_frames_t frames, lob_t out);

// get any packets that have been reassembled from incoming frames
lob_t util_frames_receive(util_frames_t frames);

// total bytes in the inbox/outbox
size_t util_frames_inlen(util_frames_t frames);
size_t util_frames_outlen(util_frames_t frames);

// meta space is (size - 14) and only filled when receiving a meta frame
util_frames_t util_frames_inbox(util_frames_t frames, uint8_t *data); // data=NULL is ready check

// fills data with the next frame, if no payload available always fills w/ meta frame, safe to re-run (idempotent)
util_frames_t util_frames_outbox(util_frames_t frames, uint8_t *data); // data=NULL is ready-check

// this must be called immediately (no inbox interleaved) after last outbox is actually sent to advance payload or clear flush request, returns if more to send
util_frames_t util_frames_sent(util_frames_t frames);

// are we active to sending/receiving frames 
util_frames_t util_frames_busy(util_frames_t frames);

// is a frame pending to be sent immediately
util_frames_t util_frames_pending(util_frames_t frames);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_serial.h"


// individual pipe local info
typedef struct pipe_serial_struct
{
  int (*read)(void);
  int (*write)(uint8_t *buf, size_t len);
  net_serial_t net;
  util_chunks_t chunks;
} *pipe_serial_t;

// do all chunk/socket stuff
pipe_t serial_flush(pipe_t pipe)
{
  int ret, count;
  uint8_t len;
  lob_t packet;
  uint8_t c1;
  pipe_serial_t to;
  if(!pipe || !(to = (pipe_serial_t)pipe->arg)) return NULL;

  count = 0;
  while((ret = to->read()) >= 0)
  {
    count++;
    c1 = ret;
    util_chunks_read(to->chunks, &c1, 1);
  }
  if(count)
  {
    LOG("read %d bytes from %s",count,pipe->id);
  }

  // any incoming full packets can be received
  while((packet = util_chunks_receive(to->chunks))) mesh_receive(to->net->mesh, packet, pipe);

  // write the next waiting chunk
  while((len = util_chunks_len(to->chunks)))
  {
    if((ret = to->write(util_chunks_write(to->chunks), len)) > 0)
    {
      LOG("wrote %d size chunk to %s",ret,pipe->id);
      // blocks till next incoming chunk is read before sending more
      util_chunks_written(to->chunks,len);
    }else{
      LOG("chunk write failed, %d != %d",ret,len);
      // TODO, write a full chunk of zeros to clear any line errors and reset state?
      break;
    }
  }

  return pipe;
}

// chunkize a packet
void serial_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_serial_t to;
  if(!pipe || !packet || !(to = (pipe_serial_t)pipe->arg)) return;

  LOG("chunking a packet of len %d",lob_len(packet));
  util_chunks_send(to->chunks, packet);
  serial_flush(pipe);
}

net_serial_t net_serial_send(net_serial_t net, const char *name, lob_t packet)
{
  if(!net || !name || !packet) return NULL;
  serial_send(xht_get(net->pipes, name), packet, NULL);
  return net;
}

pipe_t net_serial_add(net_serial_t net, const char *name, int (*read)(void), int (*write)(uint8_t *buf, size_t len), uint8_t buffer)
{
  pipe_t pipe;
  pipe_serial_t to;

  // just sanity checks
  if(!net || !name || !read || !write) return NULL;

  pipe = xht_get(net->pipes, name);
  if(!pipe)
  {
    if(!(pipe = pipe_new("serial"))) return NULL;
    if(!(pipe->arg = to = malloc(sizeof (struct pipe_serial_struct)))) return pipe_free(pipe);
    memset(to,0,sizeof (struct pipe_serial_struct));
    to->net = net;
    if(!(to->chunks = util_chunks_new(buffer)))
    {
      free(to);
      return pipe_free(pipe);
    }

    // set up pipe
    pipe->id = strdup(name);
    xht_set(net->pipes,pipe->id,pipe);
    pipe->send = serial_send;
    
  }else{
    if(!(to = (pipe_serial_t)pipe->arg)) return NULL;
  }

  // these can be modified
  to->read = read;
  to->write = write;

  return pipe;
}

net_serial_t net_serial_new(mesh_t mesh, lob_t options)
{
  net_serial_t net;
  unsigned int pipes;
  

  if(!(net = malloc(sizeof (struct net_serial_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_serial_struct));
  pipes = lob_get_uint(options,"pipes");
  if(!pipes) pipes = 3; // hashtable for active pipes
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, "net_serial", net);
  
  return net;
}

void net_serial_free(net_serial_t net)
{
  if(!net) return;
  // TODO free pipes!
  xht_free(net->pipes);
  free(net);
  return;
}

// check a single pipe's socket for any read/write activity
static void _walkflush(xht_t h, const char *key, void *val, void *arg)
{
  serial_flush((pipe_t)val);
}

net_serial_t net_serial_loop(net_serial_t net)
{
  xht_walk(net->pipes, _walkflush, NULL);
  return net;
}

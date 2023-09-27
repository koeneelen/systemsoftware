#ifndef __CONMGR_H__
#define __CONMGR_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include "sbuffer.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include "config.h"

#ifndef TIMEOUT
  #error TIMEOUT not specified!(in seconds)
#endif

typedef struct{
    tcpsock_t* socket;
    time_t last_record;
    int sensor_id;
} connection_t ;

void connmgr_listen(int port_number, sbuffer_t *sbuffer);

void connmgr_free();

#endif  //__CONMGR_H__

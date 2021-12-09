/*
 * Copyright (c) 2015, Cork Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *      Simple Energest Library File
 *      This file implements a simple Contiki Process in which information
 *      about Radio and CPU usage obtained with Contiki Energest are printed.
 *		
 *      This file is based on Simon Duquennoy's ORPL Tools and Contiki Powertrace.
 *
 * \author
 *      Pablo Corbalan <pablo.corbalan@cit.ie>
 */

#include "contiki.h"
#include "simple-energest.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
static uint16_t cnt;
static uint32_t last_cpu, last_lpm, last_tx, last_rx;
static uint32_t delta_cpu, delta_lpm, delta_tx, delta_rx;
static uint32_t curr_cpu, curr_lpm, curr_tx, curr_rx;
/*---------------------------------------------------------------------------*/
PROCESS(energest_process, "Energest Process");
/*---------------------------------------------------------------------------*/
void 
simple_energest_start(void)
{
  energest_flush();

  last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  last_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  last_rx = energest_type_time(ENERGEST_TYPE_LISTEN);

  /* Start Energest Printing Process */
  process_start(&energest_process, NULL);
}
/*---------------------------------------------------------------------------*/
void 
simple_energest_step(void)
{
  energest_flush();

  curr_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  curr_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  curr_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  curr_rx = energest_type_time(ENERGEST_TYPE_LISTEN);

  delta_cpu = curr_cpu - last_cpu;
  delta_lpm = curr_lpm - last_lpm;
  delta_tx = curr_tx - last_tx;
  delta_rx = curr_rx - last_rx;

  last_cpu = curr_cpu;
  last_lpm = curr_lpm;
  last_tx = curr_tx;
  last_rx = curr_rx;

  PRINTF("Energest: %u %lu %lu %lu %lu\n",
  	cnt++,
  	delta_cpu,
  	delta_lpm,
  	delta_tx,
  	delta_rx);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(energest_process, ev, data)
{
  static struct etimer periodic;
  PROCESS_BEGIN();
  etimer_set(&periodic, 15 * CLOCK_SECOND);
  
  while(1) {
    PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
    etimer_reset(&periodic);
    simple_energest_step();
  }

  PROCESS_END();
}
/****************************************************************//**
 *
 * @file main.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    LWIP STM32 Example Project
 *
 * (c) Deltatee Enterprises Ltd. 2013
 *
 ********************************************************************/

#include "config.h"
#include "version.h"
#include "network.h"

#include <stmlib/debug.h>
#include <stmlib/tick.h>
#include <stmlib/rand.h>

#include <cmsis/core_cmInstr.h>

#include <stdio.h>

unsigned long ticks = 0;

__attribute__((interrupt))
static void tick()
{
    tick_int_clear();
    ticks++;
}

int main()
{
    hw_init();
    debug_init();
    tick_init(tick);
    rand_init();

    printf("\n\nSTMLIB Example Code r" VERSION "\n");
    printf("System Clock: %d\n", clock_get_freq(NULL));

    network_init();

    while (1) {
        int more = network_loop(ticks);

        if (!more)
            __WFI();
    }
}

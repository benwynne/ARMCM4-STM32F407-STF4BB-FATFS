/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ChibiOS Includes */
#include "ch.h"
#include "hal.h"
/* ChibiOS Supplementary Includes */
#include "chprintf.h"
#include "shell.h"
/* FatFS */
#include "ff.h"
/* Project includes */
#include "usbcfg.h"
#include "fat.h"

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
#define TEST_WA_SIZE    THD_WORKING_AREA_SIZE(256)

#define _WHITESPACE  " \t"

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
	size_t n, size;
	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: mem\r\n");
		return;
	}
	n = chHeapStatus(NULL, &size);
	chprintf(chp, "core free memory : %u bytes\r\n", chCoreGetStatusX());
	chprintf(chp, "heap fragments   : %u\r\n", n);
	chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
        static const char *states[] = {CH_STATE_NAMES};
        thread_t *tp;

        (void)argv;
        if (argc > 0) {
                chprintf(chp, "Usage: threads\r\n");
                return;
        }
        chprintf(chp, "thread name        addr    stack prio refs     state\r\n");
        tp = chRegFirstThread();
        do {
                chprintf(chp, "%15s %08lx %08lx %4lu %4lu %9s\r\n",
                                tp->p_name, (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
                                (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
                                states[tp->p_state]);
                tp = chRegNextThread(tp);
        } while (tp != NULL);
}

static void cmd_stringtest(BaseSequentialStream *chp, int argc, char *argv[]) {

	char str[30] = "20300.1232";
	char *ptr;
	long ret1;
	double ret2;
	char *token;

	/* This function is a simple test of the string functions required for gcode_parse */

	ret1 = strtol(str, NULL, 10);

	chprintf(chp, "number [%ld]\r\n", ret1);

	ret2 = strtod(str, NULL);

	chprintf(chp, "number [%f]\r\n", ret2);

	token = strtok_r(str, _WHITESPACE, &ptr);	

}

static const ShellCommand commands[] = {
	{"mkfs", cmd_mkfs},
	{"mount", cmd_mount},
	{"unmount", cmd_unmount},
	{"getlabel", cmd_getlabel},
	{"setlabel", cmd_setlabel},
	{"tree", cmd_tree},
	{"free", cmd_free},
	{"mkdir", cmd_mkdir},
	{"hello", cmd_hello},
	{"cat", cmd_cat},
	{"mem", cmd_mem},
	{"threads", cmd_threads},
	{"bentest", cmd_bentest},
	{"stringtest", cmd_stringtest},
	{NULL, NULL}
};

static const ShellConfig shell_cfg1 = {(BaseSequentialStream *)&SDU1,  commands};

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
 * Green LED blinker thread to show the system is running.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
	(void)arg;
	chRegSetThreadName("blinker");
	while (TRUE) {
			palTogglePad(GPIOD, GPIOD_LED4);
			chThdSleep(MS2ST(250));
	}
}

/*
 * Application entry point.
 */
int main(void) {

	thread_t *shelltp = NULL;

	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured device drivers
	 *   and performs the board-specific initializations.
	 * - Kernel initialization, the main() function becomes a thread and the
	 *   RTOS is active.
	 */
	halInit();
	chSysInit();

	/*
	 * Shell manager initialization.
	 */
	shellInit();

        /*
         * Initializes a serial-over-USB CDC driver.
         */
        sduObjectInit(&SDU1);
        sduStart(&SDU1, &serusbcfg);

        /*
         * Activates the USB driver and then the USB bus pull-up on D+.
         * Note, a delay is inserted in order to not have to disconnect the cable
         * after a reset.
         */
        usbDisconnectBus(serusbcfg.usbp);
        chThdSleepMilliseconds(1000);
        usbStart(serusbcfg.usbp, &usbcfg);
        usbConnectBus(serusbcfg.usbp);



	/*
	 * Start SD Driver
	 */
	sdcStart(&SDCD1, NULL);

	/*
	 * Activate Serial Drivers 1 & 2
	 */
	//sdStart(&SDU1, NULL);
	// PA9(TX) and PA10(RX) are routed to USART1.
	//palSetPadMode(GPIOB, 6, PAL_MODE_ALTERNATE(7));
	//palSetPadMode(GPIOB, 7, PAL_MODE_ALTERNATE(7));

	// Create shell on USART 1 & 2
	shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);

	/*
	 * Creates the blinker thread.
	 */
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

        /*
         * Normal main() thread activity, in this demo it just performs
         * a shell respawn upon its termination.
         */
        while (true) {
                if (!shelltp) {
                        if (SDU1.config->usbp->state == USB_ACTIVE) {
                                /* Spawns a new shell.*/
                                shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
                        }
                }
                else {
                        /* If the previous shell exited.*/
                        if (chThdTerminatedX(shelltp)) {
                                /* Recovers memory of the previous shell.*/
                                chThdRelease(shelltp);
                                shelltp = NULL;
                        }
                }
                chThdSleepMilliseconds(500);
        }

}

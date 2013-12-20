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
#include "usb_cdc.h"
#include "fat.h"

/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/*===========================================================================*/
/* USB related stuff.                                                        */
/*===========================================================================*/

/*
 * Endpoints to be used for USBD1.
 */
#define USBD1_DATA_REQUEST_EP           1
#define USBD1_DATA_AVAILABLE_EP         1
#define USBD1_INTERRUPT_REQUEST_EP      2

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(2048)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
	size_t n, size;

	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: mem\r\n");
		return;
	}
	n = chHeapStatus(NULL, &size);
	chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
	chprintf(chp, "heap fragments   : %u\r\n", n);
	chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
	static const char *states[] = {THD_STATE_NAMES};
	Thread *tp;
	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: threads\r\n");
		return;
	}
	chprintf(chp, "    addr    stack prio refs     state time\r\n");
	tp = chRegFirstThread();
	do {
		chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n",
		         (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
		         (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
		         states[tp->p_state], (uint32_t)tp->p_time);
		tp = chRegNextThread(tp);
	} while (tp != NULL);
}

static const ShellCommand commands[] = {
	{"mount", cmd_mount},
	{"unmount", cmd_unmount},
	{"mem", cmd_mem},
	{"threads", cmd_threads},
	{"tree", cmd_tree},
	{"free", cmd_free},
	{"hello", cmd_hello},
	{"mkdir", cmd_mkdir},
	{"cat", cmd_cat},
	{"getlabel", cmd_getlabel},
	{"setlabel", cmd_setlabel},
	{NULL, NULL}
};

static const ShellConfig shell_cfg0 = {(BaseSequentialStream *)&SDU1, commands};
static const ShellConfig shell_cfg1 = {(BaseSequentialStream *)&SD1,  commands};
static const ShellConfig shell_cfg2 = {(BaseSequentialStream *)&SD2,  commands};

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
 * Green LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

	(void)arg;
	chRegSetThreadName("blinker");
	while (TRUE) {
			palTogglePad(GPIOD, GPIOD_LED4);
			chThdSleep(MS2ST(250));
	}
	return (msg_t)NULL;
}

/*
 * Application entry point.
 */
int main(void) {
	static Thread *shelltp = NULL;

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
	chThdSleepMilliseconds(1500);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);


	/*
	 * Shell manager initialization.
	 */
	shellInit();


	/*
	 * Start SD Driver
	 */
	sdcStart(&SDCD1, NULL);
	/*
	 * Activate Serial Drivers 1 & 2
	 */
	sdStart(&SD1, NULL);
	sdStart(&SD2, NULL);
	// Activates the UART driver 1, PA9(TX) and PA10(RX) are routed to USART1.
	palSetPadMode(GPIOB, 6, PAL_MODE_ALTERNATE(7));
	palSetPadMode(GPIOB, 7, PAL_MODE_ALTERNATE(7));
	// Activates the UART driver 2, PD5(TX) and PD6(RX) are routed to USART2.
	palSetPadMode(GPIOD, 5, PAL_MODE_ALTERNATE(7));
	palSetPadMode(GPIOD, 6, PAL_MODE_ALTERNATE(7));


	// Create shell on USART 1 & 2
	shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
	shellCreate(&shell_cfg2, SHELL_WA_SIZE, NORMALPRIO);

	/*
	 * Creates the blinker thread.
	 */
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

	/*
	 * Normal main() thread activity, in this demo it does nothing except
	 * sleeping in a loop and listen for events.
	 */

	while (TRUE) {
		if (!shelltp && (SDU1.config->usbp->state == USB_ACTIVE)) {
			palSetPad(GPIOD, GPIOD_LED3);
			shelltp = shellCreate(&shell_cfg0, SHELL_WA_SIZE, NORMALPRIO);
		} else if (chThdTerminated(shelltp)) {
			palClearPad(GPIOD, GPIOD_LED3);
			chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
			shelltp = NULL;           /* Triggers spawning of a new shell.        */
		}
	}
	return (int)NULL;
}

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

#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"

#include "usb_cdc.h"

#include "ff.h"

/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/**
 * @brief   Card monitor timer.
 */
static VirtualTimer tmr;

/**
 * @brief   Debounce counter.
 */
static unsigned cnt;

/**
 * @brief   Card event sources.
 */
static EventSource inserted_event, removed_event;

/**
 * @brief   Insertion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p BaseBlockDevice object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
	BaseBlockDevice *bbdp = p;

	chSysLockFromIsr();
	if (cnt > 0) {
		if (blkIsInserted(bbdp)) {
			if (--cnt == 0) {
				chEvtBroadcastI(&inserted_event);
			}
		}
		else
			cnt = POLLING_INTERVAL;
	}
	else {
		if (!blkIsInserted(bbdp)) {
			cnt = POLLING_INTERVAL;
			chEvtBroadcastI(&removed_event);
		}
	}
	chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, bbdp);
	chSysUnlockFromIsr();
}

/**
 * @brief   Polling monitor start.
 *
 * @param[in] p         pointer to an object implementing @p BaseBlockDevice
 *
 * @notapi
 */
static void tmr_init(void *p) {

	chEvtInit(&inserted_event);
	chEvtInit(&removed_event);
	chSysLock();
	cnt = POLLING_INTERVAL;
	chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, p);
	chSysUnlock();
}

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/

/**
 * @brief FS object.
 */
static FATFS SDC_FS;

/* FS mounted and ready.*/
static bool_t fs_ready = FALSE;

/* Generic large buffer.*/
static uint8_t fbuff[1024];

static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
	FRESULT res;
	FILINFO fno;
	DIR dir;
	int i;
	char *fn;

#if _USE_LFN
	fno.lfname = 0;
	fno.lfsize = 0;
#endif
	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		i = strlen(path);
		for (;;) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0)
				break;
			if (fno.fname[0] == '.')
				continue;
			fn = fno.fname;
			if (fno.fattrib & AM_DIR) {
				path[i++] = '/';
				strcpy(&path[i], fn);
				res = scan_files(chp, path);
				if (res != FR_OK)
					break;
				path[--i] = 0;
			} else {
				chprintf(chp, "%s/%s\r\n", path, fn);
			}
		}
	}
	return res;
}

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

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
	Thread *tp;

	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: test\r\n");
		return;
	}
	tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(),
	                         TestThread, chp);
	if (tp == NULL) {
		chprintf(chp, "out of memory\r\n");
		return;
	}
	chThdWait(tp);
}

static void cmd_free(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	uint32_t clusters;
	FATFS *fsp;

	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: free\r\n");
		return;
	}
	if (!fs_ready) {
		chprintf(chp, "File System not mounted\r\n");
		return;
	}
	err = f_getfree("/", &clusters, &fsp);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_getfree() failed\r\n");
		return;
	}
	chprintf(chp,"FS: %lu free clusters\r\n    %lu sectors per cluster\r\n",
		clusters, (uint32_t)SDC_FS.csize);
	chprintf(chp,"%lu B free\r\n",
		clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
	chprintf(chp,"%lu KB free\r\n",
		(clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE)/(1024));
	chprintf(chp,"%lu MB free\r\n",
		(clusters * (uint32_t)SDC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE)/(1024*1024));

}

static void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
	(void)argv;
	(void)argc;
	fbuff[0] = 0;
	scan_files(chp, (char *)fbuff);
}

static void cmd_hello(BaseSequentialStream *chp, int argc, char *argv[]) {
	FIL fsrc;   /* file object */
	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: hello\r\n");
		chprintf(chp, "       Creates hello.txt with 'Hello World'\r\n");
		return;
	}
	f_open(&fsrc, "hello.txt", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
	f_puts ("Hello World", &fsrc);
	f_close(&fsrc);
}

static void cmd_mkdir(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	if (argc == 0) {
		chprintf(chp, "Usage: mkdir dirName\r\n");
		chprintf(chp, "       Creates directory with dirName (no spaces)\r\n");
		return;
	}
	if (!fs_ready) {
		chprintf(chp, "File System not mounted\r\n");
		return;
	}
	err=f_mkdir(argv[0]);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mkdir(%s) failed\r\n",argv[0]);
		return;
	}
	return;
}

static void cmd_cat(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	FIL fsrc;   /* file object */
	char Buffer[255];
	UINT ByteToRead=sizeof(Buffer);
	UINT ByteRead;
	if (argc == 0) {
		chprintf(chp, "Usage: cat filename\r\n");
		chprintf(chp, "       Echos filename (no spaces)\r\n");
		return;
	}
	if (!fs_ready) {
		chprintf(chp, "File System not mounted\r\n");
		return;
	}
	err=f_open(&fsrc, argv[0], FA_READ);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_open(%s) failed\r\n",argv[0]);
		return;
	}
	do {
		memset(Buffer,0,sizeof(Buffer));
		err=f_read(&fsrc,Buffer,ByteToRead,&ByteRead);
		if (err != FR_OK) {
			chprintf(chp, "FS: f_read() failed\r\n");
			f_close(&fsrc);
			return;
		}
		chprintf(chp, "%s", Buffer);
	} while (ByteRead>=ByteToRead);
	chprintf(chp,"\r\n");
	f_close(&fsrc);
	return;
}

static const ShellCommand commands[] = {
	{"mem", cmd_mem},
	{"threads", cmd_threads},
	{"test", cmd_test},
	{"tree", cmd_tree},
	{"free", cmd_free},
	{"hello", cmd_hello},
	{"mkdir", cmd_mkdir},
	{"cat", cmd_cat},
	{NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
	(BaseSequentialStream *)&SDU1,
	commands
};

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
 * Card insertion event.
 */
static void InsertHandler(eventid_t id) {
	FRESULT err;

	(void)id;
	/*
	 * On insertion SDC initialization and FS mount.
	 */
	if (sdcConnect(&SDCD1))
		return;

	err = f_mount(0, &SDC_FS);
	if (err != FR_OK) {
		sdcDisconnect(&SDCD1);
		return;
	}
	fs_ready = TRUE;
}

/*
 * Card removal event.
 */
static void RemoveHandler(eventid_t id) {

	(void)id;
	sdcDisconnect(&SDCD1);
	fs_ready = FALSE;
}

/*
 * Green LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

	(void)arg;
	chRegSetThreadName("blinker");
	while (TRUE) {
		palTogglePad(GPIOD, GPIOD_LED6);
		chThdSleepMilliseconds(fs_ready ? 125 : 500);
	}
	return (msg_t)NULL;
}

/*
 * Application entry point.
 */
int main(void) {
	static Thread *shelltp = NULL;
	static const evhandler_t evhndl[] = {
		InsertHandler,
		RemoveHandler
	};
	struct EventListener el0, el1;
	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured device drivers
	 *   and performs the board-specific initializations.
	 * - Kernel initialization, the main() function becomes a thread and the
	 *   RTOS is active.
	 */
	halInit();
	chSysInit();

	palSetPad(GPIOD, GPIOD_LED3);
	chThdSleep(MS2ST(250));
	palSetPad(GPIOD, GPIOD_LED4);
	chThdSleep(MS2ST(250));
	palSetPad(GPIOD, GPIOD_LED6);
	chThdSleep(MS2ST(250));
	palSetPad(GPIOD, GPIOD_LED5);
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
	 * Activates the serial driver 6 and SDC driver 1 using default
	 * configuration.
	 */
	sdStart(&SD6, NULL);
	sdcStart(&SDCD1, NULL);

	/*
	 * Activates the card insertion monitor.
	 */
	tmr_init(&SDCD1);

	/*
	 * Creates the blinker thread.
	 */
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

	/*
	 * Creates the LWIP threads (it changes priority internally).

  chThdCreateStatic(wa_lwip_thread, LWIP_THREAD_STACK_SIZE, NORMALPRIO + 2,
                    lwip_thread, NULL);
	 */
	/*
	 * Creates the HTTP thread (it changes priority internally).

  chThdCreateStatic(wa_http_server, sizeof(wa_http_server), NORMALPRIO + 1,
                    http_server, NULL);
	 */
	/*
	 * Normal main() thread activity, in this demo it does nothing except
	 * sleeping in a loop and listen for events.
	 */
	chEvtRegister(&inserted_event, &el0, 0);
	chEvtRegister(&removed_event, &el1, 1);
	while (TRUE) {
		if (!shelltp && (SDU1.config->usbp->state == USB_ACTIVE))
			shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
		else if (chThdTerminated(shelltp)) {
			chThdRelease(shelltp);    /* Recovers memory of the previous shell.   */
			shelltp = NULL;           /* Triggers spawning of a new shell.        */
		}
		if (palReadPad(GPIOA, GPIOA_BUTTON_WKUP) != 0) {
		}
		chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(500)));
	}
	return (int)NULL;
}

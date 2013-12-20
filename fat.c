/*
 * fat.c
 *
 *  Created on: Dec 19, 2013
 *      Author: Jed Frey
 */

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/
#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"

#include "usb_cdc.h"
#include "fat.h"

#include "ff.h"

/* Generic large buffer.*/
static uint8_t fbuff[1024];

FRESULT scan_files(BaseSequentialStream *chp, char *path) {
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
				chprintf(chp, "%s\r\n", path);
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

void cmd_mount(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	(void)argc;
	(void)argv;
	err = f_mount(0, &SDC_FS);
	chprintf(chp, "Err: %d\r\n",err);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mount() failed\r\n");
		return;
	}
	sdcConnect(&SDCD1);
}

void cmd_unmount(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	(void)argc;
	(void)argv;

	sdcDisconnect(&SDCD1);
	err = f_mount(0, NULL);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mount() unmount failed\r\n");
		return;
	}
}

void cmd_free(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	uint32_t clusters;
	FATFS *fsp;
	(void)argc;
	(void)argv;

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

void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
	(void)argv;
	(void)argc;
	fbuff[0] = 0;
	scan_files(chp, (char *)fbuff);
}

void cmd_hello(BaseSequentialStream *chp, int argc, char *argv[]) {
	FIL fsrc;   /* file object */
	FRESULT err;
	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: hello\r\n");
		chprintf(chp, "       Creates hello.txt with 'Hello World'\r\n");
		return;
	}
	err = f_open(&fsrc, "hello.txt", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_open(\"hello.txt\") failed\r\n");
		return;
	} else {
		chprintf(chp, "FS: f_open(\"hello.txt\") succeeded\r\n");
	}
	err = f_puts ("Hello World", &fsrc);
	if (err == EOF) {
		chprintf(chp, "FS: f_puts(\"Hello World\",\"hello.txt\") failed\r\n");
	} else {
		chprintf(chp, "FS: f_puts(\"Hello World\",\"hello.txt\") succeeded\r\n");
	}
	f_close(&fsrc);
}

void cmd_mkdir(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	if (argc == 0) {
		chprintf(chp, "Usage: mkdir dirName\r\n");
		chprintf(chp, "       Creates directory with dirName (no spaces)\r\n");
		return;
	}
	err=f_mkdir(argv[0]);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mkdir(%s) failed\r\n",argv[0]);
		return;
	}
	return;
}

void cmd_setlabel(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	if (argc == 0) {
		chprintf(chp, "Usage: setlabel label\r\n");
		chprintf(chp, "       Sets FAT label (no spaces)\r\n");
		return;
	}
	err=f_setlabel(argv[0]);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_setlabel(%s) failed\r\n",argv[0]);
		return;
	}
	return;
}

void cmd_getlabel(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	char lbl[12];
	DWORD sn;

	if (argc > 0) {
		chprintf(chp, "Usage: getlabel\r\n");
		chprintf(chp, "       Gets and prints FAT label\r\n");
		return;
	}
	memset(lbl,0,sizeof(lbl));
	/* Get volume label of the default drive */
	err = f_getlabel("", lbl, &sn);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_getlabel failed\r\n",argv[0]);
		return;
	}
	chprintf(chp, "LABEL: %s\r\n",lbl);
	chprintf(chp, "LABEL: 0x%X\r\n",sn);
	return;
}

void cmd_cat(BaseSequentialStream *chp, int argc, char *argv[]) {
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

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
#include <stdlib.h>

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
	int fyear,fmonth,fday,fhour,fminute,fsecond;

	int i;
	char *fn;

#if _USE_LFN
	fno.lfname = 0;
	fno.lfsize = 0;
#endif
	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		i = strlen(path);
		while (true) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0)
				break;
			if (fno.fname[0] == '.')
				continue;
			fn = fno.fname;
			/*
			 * Date functions
			 */
			fyear = ((0b1111111000000000&fno.fdate) >> 9)+1980;
			fmonth= (0b0000000111100000&fno.fdate) >> 5;
			fday  = (0b0000000000011111&fno.fdate);
			/*
			 * Time functions
			 */
			fhour   = (0b1111100000000000&fno.ftime) >> 11;
			fminute = (0b0000011111100000&fno.ftime) >> 5;
			fsecond = (0b0000000000011111&fno.ftime)*2;
			chprintf(chp, "%4d-%02d-%02d %02d:%02d:%02d ", fyear, fmonth, fday, fhour, fminute, fsecond);
			if (fno.fattrib & AM_DIR) {
				path[i++] = '/';
				strcpy(&path[i], fn);
				chprintf(chp, "<DIR> %s/\r\n", path);
				res = scan_files(chp, path);
				if (res != FR_OK)
					break;
				path[--i] = 0;
			} else {
				chprintf(chp, "      %s/%s\r\n", path, fn);
			}
		}
	} else {
		chprintf(chp, "FS: f_opendir() failed\r\n");
	}
	return res;
}

void cmd_mount(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	(void)argc;
	(void)argv;
	err = f_mount(0, &SDC_FS);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mount() failed\r\n");
		return;
	}
	chprintf(chp, "FS: f_mount() succeeded\r\n");
	palSetPad(GPIOD, GPIOD_LED6);
	sdcConnect(&SDCD1);
}

void cmd_mkfs(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	int partition;
	if (argc == 0||!strcmp(argv[0],"yes")) {
		chprintf(chp, "Usage: mkfs [partition]\r\n");
		chprintf(chp, "       Formats partition [partition]\r\n");
		return;
	}
	partition=atoi(argv[0]);
	chprintf(chp, "FS: f_mkfs(%d,0,0) Started\r\n",partition);
	err = f_mkfs(partition, 0, 0);
	chprintf(chp, "FS: f_mkfs() ");
	switch (err) {
		case FR_OK:
			chprintf(chp, "succeeded: FR_OK\r\n");
			chprintf(chp, "FS: f_mkfs(%d,0,0) Finished\r\n",partition);
			break;
		case FR_DISK_ERR:
			chprintf(chp, " failed: FR_DISK_ERR\r\n");
			break;
		case FR_NOT_READY:
			chprintf(chp, " failed: FR_NOT_READY\r\n");
			break;
		case FR_WRITE_PROTECTED:
			chprintf(chp, " failed: FR_WRITE_PROTECTED\r\n");
			break;
		case FR_INVALID_DRIVE:
			chprintf(chp, "F failed: R_INVALID_DRIVE\r\n");
			break;
		case FR_NOT_ENABLED:
			chprintf(chp, " failed: FR_NOT_ENABLED\r\n");
			break;
		case FR_MKFS_ABORTED:
			chprintf(chp, " failed: FR_MKFS_ABORTED\r\n");
			break;
		case FR_INVALID_PARAMETER:
			chprintf(chp, " failed: FR_INVALID_PARAMETER\r\n");
			break;
		default:
			chprintf(chp, "%d\r\n",err);
			break;
	}
	return;
}

void cmd_unmount(BaseSequentialStream *chp, int argc, char *argv[]) {
	FRESULT err;
	(void)argc;
	(void)argv;

	palClearPad(GPIOD, GPIOD_LED6);
	sdcDisconnect(&SDCD1);
	err = f_mount(0, NULL);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_mount() unmount failed\r\n");
		return;
	}
	return;
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
	int written;

	(void)argv;
	if (argc > 0) {
		chprintf(chp, "Usage: hello\r\n");
		chprintf(chp, "       Creates hello.txt with 'Hello World'\r\n");
		return;
	}
	err = f_open(&fsrc, "hello.txt", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
	if (err != FR_OK) {
		chprintf(chp, "FS: f_open(\"hello.txt\") failed.\r\n");
		verbose_error(chp, err);
		return;
	} else {
		chprintf(chp, "FS: f_open(\"hello.txt\") succeeded\r\n");
	}
	written = f_puts ("Hello World", &fsrc);
	if (written == -1) {
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
		verbose_error(chp, err);
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
		chprintf(chp, "FS: f_setlabel(%s) failed.\r\n");
		verbose_error(chp, err);
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
		chprintf(chp, "FS: f_getlabel failed.\r\n");
		verbose_error(chp, err);
		return;
	}
	chprintf(chp, "LABEL: %s\r\n",lbl);
	chprintf(chp, "  S/N: 0x%X\r\n",sn);
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
		chprintf(chp, "FS: f_open(%s) failed.\r\n",argv[0]);
		verbose_error(chp, err);
		return;
	}
	do {
		memset(Buffer,0,sizeof(Buffer));
		err=f_read(&fsrc,Buffer,ByteToRead,&ByteRead);
		if (err != FR_OK) {
			chprintf(chp, "FS: f_read() failed\r\n");
			verbose_error(chp, err);
			f_close(&fsrc);
			return;
		}
		chprintf(chp, "%s", Buffer);
	} while (ByteRead>=ByteToRead);
	chprintf(chp,"\r\n");
	f_close(&fsrc);
	return;
}

void verbose_error(BaseSequentialStream *chp, FRESULT err) {
	chprintf(chp, "\t%s.\r\n",fresult_str(err));
}

char* fresult_str(FRESULT stat) {
	char str[255];
	memset(str,0,sizeof(str));
	switch (stat) {
		case FR_OK:
			return "Succeeded";
		case FR_DISK_ERR:
			return "A hard error occurred in the low level disk I/O layer";
		case FR_INT_ERR:
			return "Assertion failed";
		case FR_NOT_READY:
			return "The physical drive cannot work";
		case FR_NO_FILE:
			return "Could not find the file";
		case FR_NO_PATH:
			return "Could not find the path";
		case FR_INVALID_NAME:
			return "The path name format is invalid";
		case FR_DENIED:
			return "Access denied due to prohibited access or directory full";
		case FR_EXIST:
			return "Access denied due to prohibited access";
		case FR_INVALID_OBJECT:
			return "The file/directory object is invalid";
		case FR_WRITE_PROTECTED:
			return "The physical drive is write protected";
		case FR_INVALID_DRIVE:
			return "The logical drive number is invalid";
		case FR_NOT_ENABLED:
			return "The volume has no work area";
		case FR_NO_FILESYSTEM:
			return "There is no valid FAT volume";
		case FR_MKFS_ABORTED:
			return "The f_mkfs() aborted due to any parameter error";
		case FR_TIMEOUT:
			return "Could not get a grant to access the volume within defined period";
		case FR_LOCKED:
			return "The operation is rejected according to the file sharing policy";
		case FR_NOT_ENOUGH_CORE:
			return "LFN working buffer could not be allocated";
		case FR_TOO_MANY_OPEN_FILES:
			return "Number of open files > _FS_SHARE";
		case FR_INVALID_PARAMETER:
			return "Given parameter is invalid";
		default:
			return "Unknown";
	}
	return "";
}


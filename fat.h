/*
 * fat.h
 *
 *  Created on: Dec 19, 2013
 *      Author: Jed Frey
 */

#include "ff.h"

#ifndef FAT_H_
#define FAT_H_

/**
 * @brief FS object.
 */
static FATFS SDC_FS;

FRESULT scan_files(BaseSequentialStream *chp, char *path);

void cmd_mount(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_unmount(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_free(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_hello(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_mkdir(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_setlabel(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_getlabel(BaseSequentialStream *chp, int argc, char *argv[]);

void cmd_cat(BaseSequentialStream *chp, int argc, char *argv[]);

#endif /* FAT_H_ */

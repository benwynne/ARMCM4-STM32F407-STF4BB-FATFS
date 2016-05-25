/*
 * gcode_parser.c
 *
 *  Created on: 25th May 2016
 *      Author: Ben Wynne
 */

/*===========================================================================*/
/* GCODE Parser Related.                                                     */
/*===========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"

#include "usbcfg.h"
#include "fat.h"
#include "gcode_parser.h"

#include "ff.h"

// Fat FS file object
FIL fil;

void cmd_gcodetest(BaseSequentialStream *chp, int argc, char *argv[]) {
	//FIL fil;
	char line[82];
	_gcode_error_t retval;
	_param_t parsedline;

	memset(&parsedline, 0, sizeof(_param_t));

	retval = _open_job(chp, "TEST~1.GCO");	

	if(retval == GCODE_OK)
	{
		while(f_gets(line, sizeof(line), &fil))
		{	
			// process the line!
			_process_line(chp, (char *)&line, &parsedline);
			// parsedline will contain move object.
			chprintf(chp, "MOVE READY:\r\n");
			chprintf(chp, "             X[%ld]\r\n", parsedline.x);
			chprintf(chp, "             Y[%ld]\r\n", parsedline.y);
			chprintf(chp, "             Z[%ld]\r\n", parsedline.z);
			chprintf(chp, "             E[%ld]\r\n", parsedline.e);
			chprintf(chp, "             F[%ld]\r\n", parsedline.f);
		}
	}

	retval = _close_job(chp);

}

_gcode_error_t _open_job(BaseSequentialStream *chp, char *filename)
{
	FRESULT fr;
	// Mount the volume and open the specified file	
	chprintf(chp, "attempting to read job file\r\n");

	palSetPad(GPIOD, GPIOD_LED6);
	sdcConnect(&SDCD1);

	/* Register work area to the default drive */
	f_mount(&SDC_FS, "", 0);
	
	fr = f_open(&fil, filename, FA_READ);
	if(fr != FR_OK) {
		chprintf(chp, "FS: f_open() cannot open file %s\r\n", filename);
		return GCODE_ERROR;
	}

	return GCODE_OK;
}

_gcode_error_t _close_job(BaseSequentialStream *chp)
{
	FRESULT err;

	f_close(&fil);
	
	palClearPad(GPIOD, GPIOD_LED6);
	sdcDisconnect(&SDCD1);
	err = f_mount(0, "", 0);
	if(err != FR_OK) {
		chprintf(chp, "FS: f_mount() unmount failed!\r\n");
		verbose_error(chp, err);
		return GCODE_ERROR;
	}

	f_close(&fil);

	return GCODE_OK;
}

static _gcode_error_t _process_line(BaseSequentialStream *chp, char *line, _param_t *param)
{
	char *token, *ptr = NULL;
	_cmd_data_t cmd_data[_MAX_ARGS];
//	_param_t param;
	int argc = 0;

	memset(param, 0, sizeof(_param_t));

//	chprintf(chp, "Processing line [%s]\r\n", line);
//#define DEBUG_PROCESS_LINE

	memset((char *) &cmd_data[0], 0, sizeof(cmd_data));

	if((line[0] == 'G') || (line[0] == 'M'))
	{
		token = strtok_r(line, _WHITESPACE, &ptr);

		cmd_data[argc].cmd_ltr = token[0];
		cmd_data[argc].ival = strtol(token+1, NULL, 10);

		argc++;
#ifdef DEBUG_PROCESS_LINE
		chprintf(chp, "Line Start -> ");
#endif

		while(token && (argc < _MAX_ARGS))
		{
			token = strtok_r(NULL, _WHITESPACE, &ptr);
			
			if(!token)
				break;

			cmd_data[argc].cmd_ltr = token[0];


			if(strchr(token+1, '.'))
			{
				cmd_data[argc].dval = strtod(token+1, NULL);
				cmd_data[argc].is_float = true;
#ifdef DEBUG_PROCESS_LINE
//				chprintf(chp, "%c = %.06f ", cmd_data[argc].cmd_ltr, cmd_data[argc].dval);
#endif
			}
			else
			{
				cmd_data[argc].ival = strtol(token+1, NULL, 10);
#ifdef DEBUG_PROCESS_LINE
//				chprintf(chp, "%c = %d ", cmd_data[argc].cmd_ltr, cmd_data[argc].ival);
#endif
			}


			argc++;
		}
#ifdef DEBUG_PROCESS_LINE
		chprintf(chp, "\r\n");
#endif

		if(argc >= _MAX_ARGS)
		{
			chprintf(chp, "MAX ARGS reached, dropping cmd: %s\r\n", line);
			//result = GCODE_ERROR;
		}
		else
		{
			switch(cmd_data[0].cmd_ltr)
			{
				case 'G':
					switch(cmd_data[0].ival)
					{
						/* Move / Travel Move */
						case 0:
						case 1:
							param->x = (double)-1;
							param->y = (double)-1;
							param->z = (double)-1;
							param->e = (double)-1;
							param->f = (double)-1;
							_get_xyzef(chp, argc, cmd_data, param);


							// Inherit feedrate and positions from last values if
							// undefined.
							
							if(param->f == (double)-1)
								param->f = _curr_feedrate;
							else
								_curr_feedrate = param->f;

							if(param->x == (double)-1)
								param->x = _curr_x;
							else
								_curr_x = param->x;

							if(param->y == (double)-1)
								param->y = _curr_y;
							else
								_curr_y = param->y;

							if(param->z == (double)-1)
								param->z = _curr_z;
							else
								_curr_z = param->z;

							if(param->e == (double)-1)
								param->e = _curr_e;
							else
								_curr_e = param->e;
							break;
						/* Home Axis */
						case 28:
							break;
						/* Auto Level Build Platform */
						case 29:
							break;
						/* Use absolute coordinates */
						case 90:
							break;
						/* Use relative coordinates */
						case 91:
							break;
						/* Set current position */
						case 92:
							break;
						default:
							break;
						
					}
					break;
				case 'M':
					break;
				default:
					break;
			}
		}


		return(GCODE_OK);
	}
	else
	{

		chprintf(chp, "UNSUPPORTED cmd: %d\r\n", line);		

	}
	return(GCODE_OK);
}

static _gcode_error_t _get_xyzef(BaseSequentialStream *chp, int argc, _cmd_data_t *cmd_data, _param_t *param)
{
	int i;

	for(i = 1; i < argc; i++)
	{
		switch(cmd_data[i].cmd_ltr)
		{
			case 'X':
				if(!param->rel_pos)
					param->x = 0;

				if(cmd_data[i].is_float)
					param->x += GCODE_UNITS(cmd_data[i].dval);
				else
					param->x += GCODE_UNITS(cmd_data[i].ival);
				break;
			case 'Y':
				if(!param->rel_pos)
					param->y = 0;

				if(cmd_data[i].is_float)
					param->y += GCODE_UNITS(cmd_data[i].dval);
				else
					param->y += GCODE_UNITS(cmd_data[i].ival);
				break;
			case 'Z':
				if(!param->rel_pos)
					param->z = 0;
				
				if(cmd_data[i].is_float)
					param->z += GCODE_UNITS(cmd_data[i].dval);
				else
					param->z += GCODE_UNITS(cmd_data[i].ival);
				break;
			case 'E':
				if(!param->rel_pos)
					param->e = 0;

				if(cmd_data[i].is_float)
					param->e += GCODE_UNITS(cmd_data[i].dval);
				else
					param->e += GCODE_UNITS(cmd_data[i].ival);
				break;
			case 'F':
				if(cmd_data[i].is_float)
					param->f = GCODE_UNITS(cmd_data[i].dval);
				else
					param->f = GCODE_UNITS(cmd_data[i].ival);
				break;
			case 'S':
				if(cmd_data[i].is_float)
					param->target_temp[0] = FXPT(cmd_data[1].dval, 1);
				else
					param->target_temp[0] = FXPT(cmd_data[1].ival, 1);
			default:
				break;
		}
	}
	return(GCODE_OK);
}

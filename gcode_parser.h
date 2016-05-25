/*
 * gcode_parser.h
 *
 *  Created on: Dec 19, 2013
 *      Author: Jed Frey
 */

#include "ff.h"

#ifndef GCODE_PARSER_H_
#define GCODE_PARSER_H_

/**
 * @brief FS object.
 */

typedef struct
{
	char cmd_ltr;
	bool is_float;
	long ival;
	double dval;
} _cmd_data_t;

typedef struct
{
//	gmech_move_t *blk_ptr;
	int	blk_index;
	
	int ext_id;
	bool rel_pos;
	bool fan;

	int32_t x;
	int32_t y;
	int32_t z;
	int32_t e;
	int32_t f;

	int32_t curr_temp[1];
	int32_t target_temp[1];

	char *from;
} _param_t;

#define GMECH_UOM 1000 // distance is specified in 1000th of mm

#define GCODE_UOM   GMECH_UOM

#define _WHITESPACE  " \t"
#define _MAX_ARGS   10

#define FXPT(dx,p)  (int32_t) (dx*p+0.5)
#define GCODE_UNITS(dx)  FXPT(dx, GCODE_UOM)   /*  convert to gcode units */

/*!!!!!!!!!!DO NOT change order of the axis unless you know what you are doing!!!!!!!!!!!*/
typedef enum
{
        GCODE_OK,
        GCODE_ERROR
} _gcode_error_t;


// Static Variables for Value Inheritence
// TODO: Add to structure / Clean up!
static uint32_t _curr_feedrate = 0;
static uint32_t _curr_x = 0;
static uint32_t _curr_y = 0;
static uint32_t _curr_z = 0;
static uint32_t _curr_e = 0;

/* add functions here */

void cmd_gcodetest(BaseSequentialStream *chp, int argc, char *argv[]);
static _gcode_error_t _process_line(BaseSequentialStream *chp, char *line, int fd);
static _gcode_error_t _get_xyzef(BaseSequentialStream *chp, int argc, _cmd_data_t *cmd_data, _param_t *param);


#endif /* GCODE_PARSER_H_ */

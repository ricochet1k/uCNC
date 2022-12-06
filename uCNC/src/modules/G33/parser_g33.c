/*
	Name: parser_g33.c
	Description: Implements a parser extension for LinuxCNC G33 for µCNC.

	Copyright: Copyright (c) João Martins
	Author: João Martins
	Date: 25/11/2022

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/

#include "../../cnc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef ENABLE_SETTINGS_MODULES
static float tool_at_speed_tolerance;
#endif

#ifdef ENABLE_PARSER_MODULES

#ifndef UCNC_MODULE_VERSION_1_5_0_PLUS
#error "This module is not compatible with the current version of µCNC"
#endif

#define SYNC_DISABLED 0
#define SYNC_READY 1
#define SYNC_RUNNING 2
#define SYNC_EVAL 4

volatile uint8_t synched_motion_status;
volatile uint32_t spindle_index_counter;
uint32_t steps_per_index;
uint32_t motion_total_steps;

void spindle_index_cb_handler(void)
{
	switch (synched_motion_status)
	{
	case SYNC_READY:
		itp_start(false);
		spindle_index_counter = 0;
		synched_motion_status = SYNC_RUNNING;
		break;
	default:
		spindle_index_counter++;
		synched_motion_status |= SYNC_EVAL;
		break;
	}
}

// this ID must be unique for each code
#define G33 33

uint8_t g33_parse(void *args, bool *handled);
uint8_t g33_exec(void *args, bool *handled);

CREATE_EVENT_LISTENER(gcode_parse, g33_parse);
CREATE_EVENT_LISTENER(gcode_exec, g33_exec);

// this just parses and accepts the code
uint8_t g33_parse(void *args, bool *handled)
{
	gcode_parse_args_t *ptr = (gcode_parse_args_t *)args;
	if (ptr->word == 'G' && ptr->code == 33)
	{
		// stops event propagation
		*handled = true;
		if (ptr->cmd->group_extended != 0 || CHECKFLAG(ptr->cmd->groups, GCODE_GROUP_MOTION))
		{
			// there is a collision of custom gcode commands (only one per line can be processed)
			return STATUS_GCODE_MODAL_GROUP_VIOLATION;
		}
		// checks if it's G5 or G5.1
		// check mantissa
		uint8_t mantissa = (uint8_t)lroundf(((ptr->value - ptr->code) * 100.0f));

		if (mantissa != 0)
		{
			return STATUS_GCODE_UNSUPPORTED_COMMAND;
		}

		ptr->new_state->groups.motion = G33;
		ptr->new_state->groups.motion_mantissa = 0;
		SETFLAG(ptr->cmd->groups, GCODE_GROUP_MOTION);
		ptr->cmd->group_extended = EXTENDED_MOTION_GCODE(33);
		return STATUS_OK;
	}

	// if this is not catched by this parser, just send back the error so other extenders can process it
	return ptr->error;
}

// this actually performs 2 steps in 1 (validation and execution)
uint8_t g33_exec(void *args, bool *handled)
{
	gcode_exec_args_t *ptr = (gcode_exec_args_t *)args;
	if (ptr->cmd->group_extended == EXTENDED_MOTION_GCODE(33))
	{
		// stops event propagation
		*handled = true;

		if (!CHECKFLAG(ptr->cmd->words, GCODE_XYZ_AXIS))
		{
			// it's an error no axis word is specified
			return STATUS_GCODE_NO_AXIS_WORDS;
		}

		if (!CHECKFLAG(ptr->cmd->words, GCODE_WORD_K))
		{
			// it's an error no distance per rev word is specified
			return STATUS_GCODE_VALUE_WORD_MISSING;
		}

		// syncs motions and sets spindle
		if (mc_update_tools(ptr->block_data) != STATUS_OK)
		{
			return STATUS_CRITICAL_FAIL;
		}

		encoder_attach_index_cb(&spindle_index_cb_handler);

		float prev_target[AXIS_COUNT];
		mc_get_position(prev_target);
		float line_dist = 0;
		for (uint8_t i = AXIS_COUNT; i != 0;)
		{
			i--;
			prev_target[i] = ptr->target[i] - prev_target[i];
		}

		// apply any compensation transform to distance vector
		kinematics_apply_transform(prev_target);

		// determines the travelled distance
		for (uint8_t i = AXIS_COUNT; i != 0;)
		{
			i--;
			line_dist += fast_flt_pow2(prev_target[i]);
		}

		line_dist = fast_flt_sqrt(line_dist);

		// calculates the feedrate based in the K factor and the programmed spindle RPM
		// spindle is in Rev/min and K is in units(mm) per Rev Rev/min * mm/Rev = mm/min
		float total_revs = line_dist / ptr->words->ijk[2];
		ptr->block_data->feed = ptr->words->ijk[2] * ptr->block_data->spindle;
		ptr->block_data->motion_flags.bit.synched = 1;

		// queue motion in planner
		if (mc_line(ptr->target, ptr->block_data) != STATUS_OK)
		{
			return STATUS_CRITICAL_FAIL;
		}

		planner_block_t *thread = planner_get_block();
		motion_total_steps = thread->steps[thread->main_stepper];

		// calculates the expected number of steps per revolution
		steps_per_index = lroundf((float)motion_total_steps / total_revs);

		// // calculates the number of steps needed to reach constant speed
		// float junction_speed_sqr = planner_get_block_top_speed(0);
		// float accel_dist = junction_speed_sqr / thread->acceleration;

		// wait for spindle to reach the desired speed
		uint16_t programmed_speed = planner_get_spindle_speed(1);
		uint16_t at_speed_threshold = lroundf(tool_at_speed_tolerance * programmed_speed);

		// wait for tool at speed
		while (ABS(programmed_speed - tool_get_speed()) > at_speed_threshold)
		{
			if (cnc_dotasks() != STATUS_OK)
			{
				return STATUS_CRITICAL_FAIL;
			}
		}

		// flag the spindle index callback that it can start the threading motion
		synched_motion_status = SYNC_READY;

		// wait for the motion to end
		if (itp_sync() != STATUS_OK)
		{
			return STATUS_CRITICAL_FAIL;
		}

		encoder_dettach_index_cb();
	}

	return STATUS_GCODE_EXTENDED_UNSUPPORTED;
}

#endif

#ifdef ENABLE_MAIN_LOOP_MODULES
void spindle_sync_update_loop(void)
{
	// if there is new data available (new index pulse fired)
	if (synched_motion_status & SYNC_EVAL)
	{
		synched_motion_status &= ~SYNC_EVAL;

		uint32_t ideal_position = encoder_get_position(RPM_ENCODER);
		uint32_t course_position;

		// gets the number of processed steps (steps still in the planner that were not sent to the interpolator)
		planner_block_t *thread = planner_get_block();
		uint32_t processed_steps = motion_total_steps - thread->steps[thread->main_stepper];

		// calculates the expected step position based on the index counter + the intermediate sync pulse position
		__ATOMIC__
		{
			course_position = spindle_index_counter;
		}
		course_position *= steps_per_index;
		ideal_position *= steps_per_index;
		ideal_position /= RPM_PPR;
		ideal_position += course_position;

		float error = ideal_position - processed_steps;
	}
}

CREATE_EVENT_LISTENER(cnc_dotasks, spindle_sync_update_loop);
#endif

DECL_MODULE(g33)
{
#ifdef ENABLE_PARSER_MODULES
	ADD_EVENT_LISTENER(gcode_parse, g33_parse);
	ADD_EVENT_LISTENER(gcode_exec, g33_exec);
#else
#error "Parser extensions are not enabled. G33 code extension will not work."
#endif
#ifndef ENABLE_IO_MODULES
#error "IO extensions are not enabled. G33 code extension will not work."
#endif
#ifdef ENABLE_MAIN_LOOP_MODULES
	ADD_EVENT_LISTENER(cnc_dotasks, spindle_sync_update_loop);
#else
#error "Main loop extensions are not enabled. G33 code extension will not work."
#endif
}

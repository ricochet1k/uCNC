/*
	Name: grbl_interface.h
	Description: µCNC definitions and standard codes used by Grbl.

	Copyright: Copyright (c) João Martins
	Author: João Martins
	Date: 07/12/2019

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/

#ifndef GRBL_INTERFACE_H
#define GRBL_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Defines Grbl realtime ascii codes
#define CMD_CODE_RESET 0x18
#define CMD_CODE_FEED_HOLD 0x21
#define CMD_CODE_REPORT 0x3F
#define CMD_CODE_CYCLE_START 0x7E
#define CMD_CODE_SAFETY_DOOR 0x84
#define CMD_CODE_JOG_CANCEL 0x85
#define CMD_CODE_FEED_100 0x90
#define CMD_CODE_FEED_INC_COARSE 0x91
#define CMD_CODE_FEED_DEC_COARSE 0x92
#define CMD_CODE_FEED_INC_FINE 0x93
#define CMD_CODE_FEED_DEC_FINE 0x94
#define CMD_CODE_RAPIDFEED_100 0x95
#define CMD_CODE_RAPIDFEED_OVR1 0x96
#define CMD_CODE_RAPIDFEED_OVR2 0x97
#define CMD_CODE_SPINDLE_100 0x99
#define CMD_CODE_SPINDLE_INC_COARSE 0x9A
#define CMD_CODE_SPINDLE_DEC_COARSE 0x9B
#define CMD_CODE_SPINDLE_INC_FINE 0x9C
#define CMD_CODE_SPINDLE_DEC_FINE 0x9D
#define CMD_CODE_SPINDLE_TOGGLE 0x9E
#define CMD_CODE_COOL_FLD_TOGGLE 0xA0
#define CMD_CODE_COOL_MST_TOGGLE 0xA1

// Define Grbl status codes. Valid values (0-255)
#define STATUS_OK 0
#define STATUS_EXPECTED_COMMAND_LETTER 1
#define STATUS_BAD_NUMBER_FORMAT 2
#define STATUS_INVALID_STATEMENT 3
#define STATUS_NEGATIVE_VALUE 4
#define STATUS_SETTING_DISABLED 5
#define STATUS_SETTING_STEP_PULSE_MIN 6 //
#define STATUS_SETTING_READ_FAIL 7
#define STATUS_IDLE_ERROR 8
#define STATUS_SYSTEM_GC_LOCK 9
#define STATUS_SOFT_LIMIT_ERROR 10
#define STATUS_OVERFLOW 11 //
#define STATUS_MAX_STEP_RATE_EXCEEDED 12
#define STATUS_CHECK_DOOR 13
#define STATUS_LINE_LENGTH_EXCEEDED 14 //
#define STATUS_TRAVEL_EXCEEDED 15
#define STATUS_INVALID_JOG_COMMAND 16
#define STATUS_SETTING_DISABLED_LASER 17 //
#define STATUS_GCODE_UNSUPPORTED_COMMAND 20
#define STATUS_GCODE_MODAL_GROUP_VIOLATION 21
#define STATUS_GCODE_UNDEFINED_FEED_RATE 22
#define STATUS_GCODE_COMMAND_VALUE_NOT_INTEGER 23
#define STATUS_GCODE_AXIS_COMMAND_CONFLICT 24 //
#define STATUS_GCODE_WORD_REPEATED 25
#define STATUS_GCODE_NO_AXIS_WORDS 26
#define STATUS_GCODE_INVALID_LINE_NUMBER 27
#define STATUS_GCODE_VALUE_WORD_MISSING 28
#define STATUS_GCODE_UNSUPPORTED_COORD_SYS 29
#define STATUS_GCODE_G53_INVALID_MOTION_MODE 30
#define STATUS_GCODE_AXIS_WORDS_EXIST 31
#define STATUS_GCODE_NO_AXIS_WORDS_IN_PLANE 32
#define STATUS_GCODE_INVALID_TARGET 33
#define STATUS_GCODE_ARC_RADIUS_ERROR 34
#define STATUS_GCODE_NO_OFFSETS_IN_PLANE 35
#define STATUS_GCODE_UNUSED_WORDS 36					 //
#define STATUS_GCODE_G43_DYNAMIC_AXIS_ERROR 37 //
#define STATUS_GCODE_MAX_VALUE_EXCEEDED 38		 //
// additional codes
#define STATUS_BAD_COMMENT_FORMAT 39
#define STATUS_INVALID_TOOL 40
#define STATUS_FEED_NOT_SET 41
#define STATUS_SETTING_WRITE_FAIL 42
#define STATUS_SETTING_PROTECTED_FAIL 43
#define STATUS_PROGRAM_ENDED 44
#define STATUS_PROBE_SUCCESS 45
#define STATUS_GCODE_CANNED_CYCLE_MISSING_P 46
#define STATUS_GCODE_CANNED_CYCLE_MISSING_Q 47
#define STATUS_GCODE_CANNED_CYCLE_INVALID_FEEDMODE 48
#define STATUS_GCODE_CANNED_CYCLE_INVALID_RADIUSCOMPMODE 49
#define STATUS_SPINDLE_STOPPED 50
#define STATUS_TMC_CMD_MISSING_ARGS 51
#define STATUS_VFD_COMMUNICATION_FAILED 52
#define STATUS_EXTERNAL_SETTINGS_OK 53 // deprecated
#define STATUS_LASER_PPI_MODE_DISABLED 54
#define STATUS_TOOL_FAILURE 55
#define STATUS_INVALID_PLANE_SELECTED 56
#define STATUS_HARDLIMITS_DISABLED 57
#define STATUS_STREAM_FAILED 58
#define STATUS_JOG_CANCELED 59
#define STATUS_GCODE_EXTENDED_UNSUPPORTED 254 // deprecated
#define STATUS_CRITICAL_FAIL 255

// special Grbl system commands return codes
// These are not error codes but codes to print requested reports after parsing a grbl command
#define GRBL_SYSTEM_CMD 128
#define GRBL_SEND_SYSTEM_SETTINGS (GRBL_SYSTEM_CMD + 0)
#define GRBL_SEND_COORD_SYSTEM (GRBL_SYSTEM_CMD + 1)
#define GRBL_SEND_PARSER_MODES (GRBL_SYSTEM_CMD + 2)
#define GRBL_SEND_STARTUP_BLOCKS (GRBL_SYSTEM_CMD + 3)
#define GRBL_TOGGLE_CHECKMODE (GRBL_SYSTEM_CMD + 4)
#define GRBL_SEND_SETTINGS_RESET (GRBL_SYSTEM_CMD + 5)
#define GRBL_UNLOCK (GRBL_SYSTEM_CMD + 6)
#define GRBL_HOME (GRBL_SYSTEM_CMD + 7)
#define GRBL_HELP (GRBL_SYSTEM_CMD + 8)
#define GRBL_JOG_CMD (GRBL_SYSTEM_CMD + 9)

// extra commands
#define GRBL_SETTINGS_SAVED (GRBL_SYSTEM_CMD + 10)
#define GRBL_SETTINGS_LOADED (GRBL_SYSTEM_CMD + 11)
#define GRBL_SETTINGS_DEFAULT (GRBL_SYSTEM_CMD + 12)
#define GRBL_PINS_STATES (GRBL_SYSTEM_CMD + 13)
#define GRBL_SEND_SYSTEM_INFO (GRBL_SYSTEM_CMD + 14)
#define GRBL_SEND_SYSTEM_INFO_EXTENDED (GRBL_SYSTEM_CMD + 15)

#define GRBL_SYSTEM_CMD_EXTENDED (GRBL_SYSTEM_CMD + 20)
#define GRBL_SYSTEM_CMD_EXTENDED_UNSUPPORTED 253

#define EXEC_ALARM_SOFTRESET -2
#define EXEC_ALARM_EMERGENCY_STOP -1
#define EXEC_ALARM_NOALARM 0
// Grbl alarm codes. Valid values (1-255). Zero is reserved for the reset alarm.
#define EXEC_ALARM_HARD_LIMIT 1										 // hard limits hit while in motion other then homing
#define EXEC_ALARM_SOFT_LIMIT 2										 // target is off bounds of the machine kinematics
#define EXEC_ALARM_ABORT_CYCLE 3									 // an abort command was issued
#define EXEC_ALARM_PROBE_FAIL_INITIAL 4						 // probe was already triggered and was not able to initialize probing
#define EXEC_ALARM_PROBE_FAIL_CONTACT 5						 // probe failed to triggered before reaching the limit target
#define EXEC_ALARM_HOMING_FAIL_RESET 6						 // homing was aborted by a reset command
#define EXEC_ALARM_HOMING_FAIL_DOOR 7							 // door was opened during homing motion
#define EXEC_ALARM_HOMING_FAIL_PULLOFF 8					 // homing limits failed to normalize after retract by pull-distance
#define EXEC_ALARM_HOMING_FAIL_APPROACH 9					 // homing limits failed make initial contact
#define EXEC_ALARM_HOMING_FAIL_DUAL_APPROACH 10		 // homing limits failed make initial contact (self squaring)
#define EXEC_ALARM_HOMING_FAIL_LIMIT_ACTIVE 11		 // homing could not start since one of the limits was already triggered
#define EXEC_ALARM_SPINDLE_SYNC_FAIL 12						 // failed to achieve spindle sync speed
#define EXEC_ALARM_HARD_LIMIT_NOMOTION 13					 // hard limits were triggered without any motion (position was not lost)
#define EXEC_ALARM_PLASMA_THC_ARC_START_FAILURE 14 // failed to start arc with plasma THC

// formated messages
#define STR_EOL "\r\n"
#define MSG_EOL __romstr__(STR_EOL)
#define MSG_OK __romstr__("ok")
#define MSG_ERROR __romstr__("error:")
#define MSG_ALARM __romstr__("ALARM:")
#define MSG_ECHO __romstr__("[echo:")
#if EMULATE_GRBL_STARTUP == 0
#define MSG_STARTUP_START "uCNC "
#define MSG_STARTUP_END " ['$' for help]"
#elif EMULATE_GRBL_STARTUP == 1
#define MSG_STARTUP_START "Grbl "
#define MSG_STARTUP_END " [uCNC v" CNC_VERSION " '$' for help]"
#elif EMULATE_GRBL_STARTUP == 2
#define MSG_STARTUP __romstr__("Grbl 1.1f ['$' for help]" STR_EOL)
#endif
#ifndef MSG_STARTUP
#define MSG_STARTUP __romstr__(MSG_STARTUP_START CNC_MAJOR_MINOR_VERSION MSG_STARTUP_END STR_EOL)
#endif
#define MSG_HELP __romstr__("[HLP:$$ $# $G $I $N $x=val $Nx=line $J=line $C $X $H ~ ! ? ctrl-x]" STR_EOL)

// Non query feedback messages
#define MSG_START __romstr__("//MSG:")
#define MSG_END __romstr__(STR_EOL)
#define MSG_FEEDBACK_1 __romstr__("Reset to continue")
#define MSG_FEEDBACK_2 __romstr__("'$H'|'$X' to unlock")
#define MSG_FEEDBACK_3 __romstr__("Caution: Unlocked")
#define MSG_FEEDBACK_4 __romstr__("Enabled")
#define MSG_FEEDBACK_5 __romstr__("Disabled")
#define MSG_FEEDBACK_6 __romstr__("Check Door")
#define MSG_FEEDBACK_7 __romstr__("Check Limits")
#define MSG_FEEDBACK_8 __romstr__("Pgm End")
#define MSG_FEEDBACK_9 __romstr__("Restoring defaults")
#define MSG_FEEDBACK_10 __romstr__("Restoring spindle")
// #define MSG_FEEDBACK_11 __romstr__("Sleeping") not implemented
/*NEW*/
#define MSG_FEEDBACK_12 __romstr__("Check Emergency stop")
#define MSG_FEEDBACK_13 __romstr__("Settings saved")
#define MSG_FEEDBACK_14 __romstr__("Settings loaded")
#define MSG_FEEDBACK_15 __romstr__("Settings defaults")

#define MSG_STATUS_ALARM __romstr__("Alarm")
#define MSG_STATUS_DOOR __romstr__("Door")
#define MSG_STATUS_HOLD __romstr__("Hold")
#define MSG_STATUS_HOME __romstr__("Home")
#define MSG_STATUS_JOG __romstr__("Jog")
#define MSG_STATUS_RUN __romstr__("Run")
#define MSG_STATUS_IDLE __romstr__("Idle")
#define MSG_STATUS_CHECK __romstr__("Check")

#define MSG_STATUS_MPOS __romstr__("|MPos:")
#define MSG_STATUS_WPOS __romstr__("|WPos:")
#define MSG_STATUS_FS __romstr__("|FS:")
#define MSG_STATUS_F __romstr__("|F:")
#define MSG_STATUS_WCO __romstr__("|WCO:")
#define MSG_STATUS_OVR __romstr__("|Ov:")
#define MSG_STATUS_TOOL __romstr__("|A:")
#define MSG_STATUS_LINE __romstr__("|Ln:")
#define MSG_STATUS_PIN __romstr__("|Pn:")
#define MSG_STATUS_BUF __romstr__("|Buf:")

	// #define MSG_INT "%d"
	// #define MSG_FLT "%0.3f"
	// #define MSG_FLT_IMPERIAL "%0.5f"

#ifdef __cplusplus
}
#endif

#endif

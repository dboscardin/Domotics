#ifndef PROTOCOL_H
#define PROTOCOL_H

#define STATUS_OK               0
#define ERR_DEVICE_NOT_FOUND    1
#define ERR_INVALUD_COMMAND
#define ERR_LINK_FAILED            3
#define ERR_DEVICE_TYPE_MISMATCH   4
#define STATUS_MANUAL_OVERRIDE     5
#define ERR_DEVICE_CRASHED         6
#define ERR_INVALID_PARAM          7
#define ERR_CYCLE_DETECTED         8

#define CMD_QUERY        "QUERY"
#define CMD_SWITCH       "SWITCH"
#define CMD_SET          "SET"
#define CMD_LINK_CHILD   "LINK_CHILD"
#define CMD_UNLINK_CHILD "UNLINK_CHILD"
#define CMD_SET_PARENT   "SET_PARENT"
#define CMD_TERMINATE    "TERMINATE"
 
#define MAX_MSG_LEN 512
#define FIFO_PATH_FMT "/tmp/domotica_%d.fifo"

#endif

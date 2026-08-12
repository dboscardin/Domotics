#ifndef PROTOCOL_H
#define PROTOCOL_H

//STATUS
#define STATUS_OK                  0
#define ERR_DEVICE_NOT_FOUND       1
#define ERR_INVALID_COMMAND        2
#define ERR_LINK_FAILED            3
#define ERR_DEVICE_TYPE_MISMATCH   4
#define STATUS_MANUAL_OVERRIDE     5
#define ERR_DEVICE_CRASHED         6
#define ERR_INVALID_PARAM          7
#define ERR_CYCLE_DETECTED         8
#define ERR_NOT_FOUND              9

//COMMAND
#define CMD_INFO            "INFO"
#define CMD_SWITCH          "SWITCH"
#define CMD_SET             "SET"
#define CMD_LINK_CHILD      "LINK"
#define CMD_UNLINK_CHILD    "UNLINK"
#define CMD_SET_PARENT      "SET_PARENT"
#define CMD_DELETE          "DELETE"
#define CMD_MIRROR          "MIRROR"
#define CMD_MIRROR_RESP     "MIRROR_RESP"
#define CMD_CHILD_DIED      "CHILD_DIED"
 
#define MAX_MSG_LEN 512
#define FIFO_PATH_FMT "/tmp/domotica_%s_%d.fifo"
#define FIFO_CONTROLLER "/tmp/domotica_controller_0.fifo"

#define RESP_FORMAT "%d %s"
#define RESP_FORMAT_NO_PAYLOAD "%d" 

#define FIELD_SEP ';'
#define KV_SEP '='
//ES: 0 power=on;time=45

#endif

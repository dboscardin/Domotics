#!/bin/bash

# Standard error codes
STATUS_OK=0
ERR_DEVICE_NOT_FOUND=1
ERR_INVALID_COMMAND=2
ERR_LINK_FAILED=3
ERR_DEVICE_TYPE_MISMATCH=4
STATUS_MANUAL_OVERRIDE=5
ERR_DEVICE_CRASHED=6
ERR_INVALID_PARAM=7
ERR_CYCLE_DETECTED=8
ERR_NOT_FOUND=9
ERR_SELF_LINK=10
ERR_TIMEOUT=11
ERR_RESOURCE_ERROR=12
ERR_PERMISSION_ERROR=13

# Check the 2 parameters
if [ "$#" -lt 2 ]; then

    echo "Use: $0 <id> <command> [parameters]"
    echo "Example:"
    echo " $0 1 switch power on/off "
    echo " $0 1 delete"
    echo " $0 1 info"
    exit "$ERR_INVALID_COMMAND"
fi

# Check if id parameter is a number
if ! [[ "$1" =~ ^[0-9]+$ ]]; then
    echo "Error: param <id> must be a number"
    exit "$ERR_INVALID_PARAM"
fi

# Variable assignments
ID=$1
COMMAND="$2"
shift 2 # To get the remaining parameters
PARAMETERS="$@"

# Convert to uppercase
COMMAND=$(echo "$COMMAND" | tr '[:lower:]' '[:upper:]')

# Command whitelist
if [ "$COMMAND" != "SWITCH" ] && [ "$COMMAND" != "DELETE" ] && [ "$COMMAND" != "INFO" ]; then
    echo "Error: command '$COMMAND' is not supported."
    echo "Supported commands are: switch, info, delete"
    exit "$ERR_INVALID_COMMAND"
fi

# Check parameters for delete and info
if { [ "$COMMAND" = "DELETE" ] || [ "$COMMAND" = "INFO" ]; } && [ -n "$PARAMETERS" ]; then
    echo "Error: command '$COMMAND' does not support any parameters."
    exit "$ERR_INVALID_PARAM"
fi


# Check parameters for switch
if [ "$COMMAND" = "SWITCH" ]; then
    # Extract words from parameters
    read -r LABEL POS EXTRA <<< "$PARAMETERS"

    # Verify that there are exactly two parameters
    if [ -z "$LABEL" ] || [ -z "$POS" ] || [ -n "$EXTRA" ]; then
        echo "Error: command 'switch' requires exactly 2 parameters (<label> <value>)."
        exit "$ERR_INVALID_PARAM"
    fi

    # whitelist

    LABEL_LOWER=$(echo "$LABEL" | tr '[:upper:]' '[:lower:]')
    POS_LOWER=$(echo "$POS" | tr '[:upper:]' '[:lower:]')
    case "$LABEL_LOWER" in
        power|open|close|main)
            # Only accepts on/off
            if [ "$POS_LOWER" != "on" ] && [ "$POS_LOWER" != "off" ]; then
                echo "Error: value for '$LABEL_LOWER' must be 'on' or 'off'."
                exit "$ERR_INVALID_PARAM"
            fi

            POS=$POS_LOWER # For safety
            ;;
        
        delay|perc|thermostat|temp)
            # Check if pos is a number
            if ! [[ "$POS" =~ ^-?[0-9]+$ ]]; then
                echo "Error: value for '$LABEL_LOWER' must be an integer number."
                exit "$ERR_INVALID_PARAM"
            fi

            # Percentage must be between 0 and 100
            if [ "$LABEL_LOWER" = "perc" ]; then
                if [ "$POS" -lt 0 ] || [ "$POS" -gt 100 ]; then
                    echo "Error: percentage (perc) must be between 0 and 100."
                    exit "$ERR_INVALID_PARAM"
                fi
            fi
            ;;
        begin|end)
            # Check time in HH:MM format
            if ! [[ "$POS" =~ ^([0-1][0-9]|2[0-3]):[0-5][0-9]$ ]]; then
                echo "Error: value for '$LABEL_LOWER' must be in a valid HH:MM format (e.g., 14:30)."
                exit "$ERR_INVALID_PARAM"
            fi
            ;;
        *)
            # Label does not exist
            echo "Error: invalid switch label '$LABEL'."
            echo "Supported labels: power, open, close, delay, perc, thermostat, begin, end"
            exit "$ERR_INVALID_PARAM"
            ;;
        

    esac

    PARAMETERS="$LABEL_LOWER $POS"

fi



# Create the command to send to the device
if [ -z "$PARAMETERS" ]; then
    CMD="$COMMAND"
else
    CMD="$COMMAND $PARAMETERS MANUAL"
fi

# Search for the FIFO
FIFO_PATH=$(ls /tmp/domotica_*_${ID}.fifo 2>/dev/null)

# Verify that it actually exists (-p checks if the file is a named pipe)
if [ -z "$FIFO_PATH" ] || [ ! -p "$FIFO_PATH" ]; then 
    echo "Error: device not found with ID:$ID"
    exit "$ERR_DEVICE_NOT_FOUND"
fi

# Send the command to the FIFO
echo "$CMD" > "$FIFO_PATH"

# Check if the operation succeeded
if [ $? -eq 0 ]; then
    echo "Command $CMD sent successfully to Device ID: $ID"
    exit "$STATUS_OK"
else 
    echo "Error occurred during write into FIFO: $FIFO_PATH"
    exit "$ERR_INVALID_COMMAND"
fi
#!/bin/bash

#Codici di errore
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

#controllo dei 2 parametri obbligatori
if [ "$#" -lt 2 ]; then

    #TODO: rivedere lista comandi
    echo "Use: $0 <id> <command> [parameters]"
    echo "Example:"
    echo " $0 1 switch power on/off "
    echo " $0 1 delete"
    echo " $0 1 info"
    exit "$ERR_INVALID_COMMAND"
fi

#controllo se il parametro id è un numero
if ! [[ "$1" =~ ^[0-9]+$ ]]; then
    echo "Error: param <id> must be a number"
    exit "$ERR_INVALID_PARAM"
fi

#prendo i parametri
ID=$1
COMMAND="$2"
shift 2 #per prendere i parametri finali
PARAMETERS="$@"

#conversione in maiscolo
COMMAND=$(echo "$COMMAND" | tr '[:lower:]' '[:upper:]')

#whitelist dei comandi
if [ "$COMMAND" != "SWITCH" ] && [ "$COMMAND" != "DELETE" ] && [ "$COMMAND" != "INFO" ]; then
    echo "Error: command '$COMMAND' is not supported."
    echo "Supported commands are: switch, info, delete"
    exit "$ERR_INVALID_COMMAND"
fi

#controllo parametri per delete e info
if { [ "$COMMAND" = "DELETE" ] || [ "$COMMAND" = "INFO" ]; } && [ -n "$PARAMETERS" ]; then
    echo "Error: command '$COMMAND' does not support any parameters."
    exit "$ERR_INVALID_PARAM"
fi


#controllo parametri per switch
if [ "$COMMAND" = "SWITCH" ]; then
    #estraggo quante parole ci sono in parameters
    read -r LABEL POS EXTRA <<< "$PARAMETERS"

    #verifico che si siano esattamente due parametri
    if [ -z "$LABEL" ] || [ -z "$POS" ] || [ -n "$EXTRA" ]; then
        echo "Error: command 'switch' requires exactly 2 parameters (<label> <value>)."
        exit "$ERR_INVALID_PARAM"
    fi

    #whitelist delle label

    LABEL_LOWER=$(echo "$LABEL" | tr '[:upper:]' '[:lower:]')
    POS_LOWER=$(echo "$POS" | tr '[:upper:]' '[:lower:]')
    case "$LABEL_LOWER" in
        power|open|close|main)
            #acceta solo on/off
            if [ "$POS_LOWER" != "on" ] && [ "$POS_LOWER" != "off" ]; then
                echo "Error: value for '$LABEL_LOWER' must be 'on' or 'off'."
                exit "$ERR_INVALID_PARAM"
            fi

            POS=$POS_LOWER #per sicurezza
            ;;
        
        delay|perc|thermostat|temp)
            #controllo se pos è un numero
            if ! [[ "$POS" =~ ^-?[0-9]+$ ]]; then
                echo "Error: value for '$LABEL_LOWER' must be an integer number."
                exit "$ERR_INVALID_PARAM"
            fi

            #la percentuale deve essere tra 0 e 100
            if [ "$LABEL_LOWER" = "perc" ]; then
                if [ "$POS" -lt 0 ] || [ "$POS" -gt 100 ]; then
                    echo "Error: percentage (perc) must be between 0 and 100."
                    exit "$ERR_INVALID_PARAM"
                fi
            fi
            ;;
        begin|end)
            # controllo orario formato HH:MM
            if ! [[ "$POS" =~ ^([0-1][0-9]|2[0-3]):[0-5][0-9]$ ]]; then
                echo "Error: value for '$LABEL_LOWER' must be in a valid HH:MM format (e.g., 14:30)."
                exit "$ERR_INVALID_PARAM"
            fi
            ;;
        *)
            # label non esiste
            echo "Error: invalid switch label '$LABEL'."
            echo "Supported labels: power, open, close, delay, perc, thermostat, begin, end"
            exit "$ERR_INVALID_PARAM"
            ;;
        

    esac

    PARAMETERS="$LABEL_LOWER $POS"

fi



#Creo il comando da mandare al dispositivo
if [ -z "$PARAMETERS" ]; then
    CMD="$COMMAND"
else
    CMD="$COMMAND $PARAMETERS MANUAL"
fi

#cerco la fifo
FIFO_PATH=$(ls /tmp/domotica_*_${ID}.fifo 2>/dev/null)

#verifico se esiste davvero (-p controlla se il file è un named pipe)
if [ -z "$FIFO_PATH" ] || [ ! -p "$FIFO_PATH" ]; then 
    echo "Error: device not found with ID:$ID"
    exit "$ERR_DEVICE_NOT_FOUND"
fi

#invio il comando alla fifo
echo "$CMD" > "$FIFO_PATH"

#controllo se è andato a buon fine
if [ $? -eq 0 ]; then
    echo "Command $CMD sent successfully to Device ID: $ID"
    exit "$STATUS_OK"
else 
    echo "Error occurred during write into FIFO: $FIFO_PATH"
    exit "$ERR_INVALID_COMMAND"
fi
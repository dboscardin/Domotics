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
if ! [ "$1" -eq "$1" ] 2>/dev/null; then
    echo "Error: param <id> must be a number"
    exit "$ERR_INVALID_PARAM"
fi

#prendo i parametri
ID=$1
COMMAND="$2"

#whitelist dei comandi
if [ "$COMMAND" != "switch" ] && [ "$COMMAND" != "delete" ] && [ "$COMMAND" != "info" ]; then
    echo "Error: command '$COMMAND' is not supported."
    echo "Supported commands are: switch, info, delete"
    exit "$ERR_INVALID_COMMAND"
fi

#conversione in maiscolo
COMMAND=$(echo "$COMMAND" | tr '[:lower:]' '[:upper:]')

shift 2 #per prendere i parametri finali
PARAMETERS="$@"

#Creo il comando da mandare al dispositivo
if [ -z "$PARAMETERS" ]; then
    CMD="$COMMAND"
else
    CMD="$COMMAND $PARAMETERS"
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
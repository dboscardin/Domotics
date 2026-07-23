#!/bin/bash

#controllo dei 2 parametri obbligatori
if [ "$#" -lt 2 ]; then

    #TODO: rivedere lista comandi
    echo "Use: $0 <id> <command> [parameters]"
    echo "Example:"
    echo " $0 1 switch power on/off "
    echo " $0 1 DELETE"
    exit 1
fi

#controllo se il parametro id è un numero
if !([ "$1" -eq "$1" ] 2>/dev/null); then
    echo "Error: param <id> must be a number"
    exit 1
fi

#prendo i parametri
ID=$1
shift #cosi prendo tutto il comando senza id
COMMAND="$*"

#cerco la fifo
FIFO_PATH=$(ls /tmp/domotica_*_${ID}.fifo 2>/dev/null)

#verifico se esiste davvero (-p controlla se il file è un named pipe)
if [ -z "$FIFO_PATH" ] || [ ! -p "$FIFO_PATH" ]; then 
    echo "Error: device not found with ID:$ID"
    exit 1
fi

#invio il comando alla fifo
echo "$COMMAND" > "$FIFO_PATH"

#controllo se è andato a buon fine
if [ $? -eq 0 ]; then
    echo "Command $COMMAND sent successfully to FIFO: $FIFO_PATH"
else 
    echo "Error occurred during write into FIFO: $FIFO_PATH"
    exit 1
fi
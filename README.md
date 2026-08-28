# Domotics

The project implements an emulated home automation system, in which each device is represented by a **distinct UNIX process**, spawned by the Controller, our root process. Processes communicate through **POSIX named pipes (FIFOs)**, exchanging messages according
to a previously defined protocol.

## User guide and commands

### Controller's interactive shell

| Command | Description | Example |
| :--- | :--- | :--- |
| `list` | Lists all devices, PID, type and logical link. | `list` |
| `add <device>` | Creates a new process (`bulb`, `window`, `fridge`, `hub`, `timer`). | `add bulb` |
| `del <id>` | Terminates a process (if Control Device, deletes on cascade his children). | `del 1` |
| `link <id1> to <id2>` | Logically link `id1` device to `id2` device. | `link 3 to 1` |
| `unlink <id1> from <id2>`| Unlink `id1` device from the parent `id2` | `unlink 3 from 1` |
| `switch <id> <label> <pos>` | Sets the switch/registry `<label>` of `<id>` device | `switch 3 power on` |
| `info <id>` | Shows details about <id> device's state. | `info 1` |
| `cmds` | Shows available commands list. | `cmds` |
| `quit` | Exits the app and releases the resources. | `quit` |

### Manual interaction script (Bypass Controller)
Simulates a direct physical action performed locally on the device (e.g., pressing a physical button or manually changing refrigerator settings):

```bash
./manual_interaction.sh <id> <command> [parameters]
```

**Examples:**
```bash
# Turning a bulb on/off
./manual_interaction.sh 3 switch power on

# Open/close window
./manual_interaction.sh 4 switch open on

# Manual override of the fridge's protected registers
./manual_interaction.sh 5 switch perc 85
./manual_interaction.sh 5 switch thermostat 4

# Request of info or direct cancellation
./manual_interaction.sh 3 info
./manual_interaction.sh 3 delete
```

---

## Compilation and Execution

The project includes a POSIX-compliant `Makefile`.

### 1. Compilation
```bash
make build
# or:
make
```

### 2. Standard Execution
Launch the system's interactive shell:
```bash
make run
# or:
./domotics
```

### 3. Execution with scenario
Executes a predefined sequence of commands:
```bash
make run ARGS=scenario.txt
```

### 4. Pulizia
Removes compiled object files and the executable, and cleans up any FIFO pipes left in `/tmp`:
```bash
make clean
```

---

## Repository structure

```
.
├── Makefile                # Build, clean e run rules
├── README.md               # Project documentation
├── manual_interaction.sh   # Script bash for manual override
├── scenario.txt            # Demonstration test scenario
└── code/
    ├── main.c              # Entry point
    ├── controller.h / .c   # Main process and controller shell
    ├── device.h / .c       # Types and utility common to all devices
    ├── hub.h / .c  
    ├── timer.h / .c
    ├── bulb.h / .c 
    ├── window.h / .c
    ├── fridge.h / .c
    ├── ipc.h / .c          # FIFO and messages management
    └── protocol.h          # const and error codes
```
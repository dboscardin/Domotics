# Sistema di Domotica Emulato (Domotics)
**Progetto per il Laboratorio del Corso di Sistemi Operativi**

Sistema di domotica modulare e concorrente in cui ogni dispositivo è rappresentato da un **processo OS autonomo**. La gerarchia tra i dispositivi è puramente **logica** ed è gestita mediante instradamento dei messaggi tramite **Inter-Process Communication (IPC)** su *named pipes* (FIFO POSIX).

---

## 🏛️ Architettura del Sistema

```
                         [ Controller (ID 0) ]
                       (Thread Main + Listener)
                                  │
                 ┌────────────────┼────────────────┐
                 ▼                ▼                ▼
           [ Hub (ID 1) ]   [ Timer (ID 2) ]  [ Fridge (ID 5) ]
                 │                │
           ┌─────┴─────┐          ▼
           ▼           ▼    [ Bulb (ID 6) ]
     [ Bulb (ID 3) ] [ Window (ID 4) ]
```

- **Gerarchia OS Piatta**: Tutti i processi dei dispositivi sono figli diretti (`fork`) del processo radice **Controller**.
- **Gerarchia Logica**: Definita a runtime mediante scambio di messaggi IPC (`link <id1> to <id2>`), senza riavviare i processi.
- **Canali IPC**: Ogni dispositivo crea e ascolta su una FIFO dedicata `/tmp/domotica_<tipo>_<id>.fifo`. Il Controller ascolta su `/tmp/domotica_controller_0.fifo`.
- **Concorrenza & Multithreading**:
  - Il **Controller** impiega un'architettura multithread con `pthreads`: il *Main Thread* legge i comandi da tastiera (`stdin`), mentre il *Listener Thread* riceve le risposte asincrone dalla FIFO.
  - Mascheramento sicuro di `SIGCHLD` (`sigprocmask`) per prevenire race condition durante la manipolazione delle strutture dati.
  - Simulazione dei tempi di elaborazione (`ipc_simulate_delay`) compresi tra 1 e 3 secondi per testare la concorrenza.

---

## 🔌 Dispositivi Supportati

### 1. Control Devices (Dispositivi di Controllo)
- **Controller (ID 0)**:
  - *Stato*: `On`/`Off` (interruttore `main`).
  - *Registro*: `num` (numero di figli logici diretti collegati).
- **Hub**:
  - Consente di raggruppare dispositivi in parallelo e propagare i comandi a cascata.
  - *Mirroring*: interroga attivamente i figli via IPC e riassume lo stato complessivo. Se i figli presentano stati discordanti, riporta lo stato `Manual_Override`. Un nuovo comando all'Hub ripristina la consistenza eliminando l'override.
- **Timer**:
  - Programmazione temporale (registri `begin` ed `end` in formato `HH:MM`) per un dispositivo o ramo collegato.
  - Esegue accensione/spegnimento automatico in base all'orologio di sistema e notifica il Controller.

### 2. Interaction Devices (Dispositivi Interattivi)
- **Bulb**:
  - *Stato*: `On`/`Off` (interruttore `power`).
  - *Registro*: `time` (tempo totale di accensione in secondi, tracciato con `CLOCK_MONOTONIC`).
- **Window**:
  - *Stato*: `Open`/`Closed` (interruttori `open` e `close`).
  - *Switch a molla*: i comandi modificano lo stato ma ritornano automaticamente in posizione inattiva.
  - *Registro*: `time` (tempo totale di apertura).
- **Fridge**:
  - *Stato*: `Open`/`Closed` (interruttori `open` e `close`).
  - *Registri*:
    - `time` (tempo totale di apertura).
    - `delay` (tempo in secondi per l'auto-chiusura automatica della porta).
    - `temp` (temperatura interna corrente).
    - `perc` (% di riempimento, 0-100) — *Modificabile solo manualmente*.
    - `thermostat` (temperatura target) — *Modificabile solo manualmente*.

---

## 💻 Guida all'Uso e Comandi

### Shell Interattiva del Controller

| Comando | Descrizione | Esempio |
| :--- | :--- | :--- |
| `list` | Elenca tutti i dispositivi, PID, tipo e collegamento logico | `list` |
| `add <device>` | Istanzia un nuovo processo (`bulb`, `window`, `fridge`, `hub`, `timer`) | `add bulb` |
| `del <id>` | Termina un dispositivo (se Control Device, cancella a cascata i figli) | `del 1` |
| `link <id1> to <id2>` | Collega logicamente il dispositivo `id1` sotto il controllo di `id2` | `link 3 to 1` |
| `unlink <id1> from <id2>`| Scollega il dispositivo `id1` dal genitore `id2` | `unlink 3 from 1` |
| `switch <id> <label> <pos>` | Imposta lo switch/registro `<label>` del dispositivo `<id>` | `switch 3 power on` |
| `info <id>` | Mostra le informazioni dettagliate e lo stato del dispositivo | `info 1` |
| `cmds` | Mostra la lista dei comandi disponibili | `cmds` |
| `quit` | Termina l'applicazione e rilascia tutte le risorse | `quit` |

### Script di Interazione Manuale (Bypass Controller)
Simula un'azione fisica diretta eseguita localmente sul dispositivo (es. pressione del pulsante fisico o modifica manuale dei parametri del frigo):

```bash
./manual_interaction.sh <id> <command> [parameters]
```

**Esempi:**
```bash
# Accensione/spegnimento manuale di una lampadina
./manual_interaction.sh 3 switch power on

# Apertura/chiusura finestra
./manual_interaction.sh 4 switch open on

# Modifica manuale dei registri protetti del frigorifero
./manual_interaction.sh 5 switch perc 85
./manual_interaction.sh 5 switch thermostat 4

# Richiesta info o cancellazione diretta
./manual_interaction.sh 3 info
./manual_interaction.sh 3 delete
```

---

## 🛠️ Compilazione ed Esecuzione

Il progetto include un `Makefile` conforme agli standard POSIX.

### 1. Compilazione
```bash
make build
# oppure semplicemente:
make
```

### 2. Esecuzione Standard
Avvia la shell interattiva del sistema:
```bash
make run
# oppure:
./domotics
```

### 3. Esecuzione con Scenario
Esegue una sequenza predefinita di comandi (utile per dimostrazioni o test automatici) e consente di proseguire in modalità interattiva:
```bash
make run SCENARIO=scenario.txt
```

### 4. Pulizia
Rimuove i file oggetto compilati, l'eseguibile e pulisce eventuali pipe FIFO rimaste in `/tmp`:
```bash
make clean
```

---

## 📁 Struttura della Repository

```
.
├── Makefile                # Regole di build, clean e run
├── README.md               # Documentazione del progetto
├── manual_interaction.sh   # Script bash per interazione manuale (override)
├── scenario.txt            # Scenario dimostrativo di test
└── code/                   # Sorgenti C
    ├── main.c              # Entry point del programma
    ├── controller.h / .c   # Processo principale Controller e shell interattiva
    ├── device.h / .c       # Tipi e utility comuni a tutti i dispositivi
    ├── hub.h / .c          # Logica del dispositivo di controllo Hub
    ├── timer.h / .c        # Logica del dispositivo di controllo Timer
    ├── bulb.h / .c         # Dispositivo Lampadina
    ├── window.h / .c       # Dispositivo Finestra
    ├── fridge.h / .c       # Dispositivo Frigorifero
    ├── ipc.h / .c          # Wrapper per gestione FIFO e messaggistica
    └── protocol.h          # Costanti di protocollo e codici di errore
```
🖥️ Linux System Monitor — C++ Kernel Diagnostics Suite

> A collection of low-level Linux system monitoring tools written in C++, reading directly from `/proc` virtual filesystem. No third-party libraries except NCurses for terminal UI.

---

📦 Project Structure

```
.
├── task3_color_alerts.cpp   # Dynamic color-coded resource bars (NCurses)
├── task4_logger.cpp         # Automated CSV logging engine (sys_report.log)
├── task5_uptime_ctx.cpp     # Context switch rate & uptime formatter
└── README.md
```

---

⚙️ Prerequisites

| Dependency | Purpose | Install |
|---|---|---|
| `g++` (C++17) | Compiler | `sudo apt install build-essential` |
| `libncurses-dev` | Terminal UI (Task 3 only) | `sudo apt install libncurses-dev` |
| Linux kernel | `/proc` filesystem access | — |

---

🟢 Task 3 — Dynamic Visual Color Alerts

**File:** `task3_color_alerts.cpp`

Implements a live NCurses terminal dashboard where resource bars **shift color dynamically** based on current kernel workload.

How it works

- Samples CPU usage via a 200 ms delta across two `/proc/stat` reads
- Reads RAM from `/proc/meminfo` (`MemTotal` vs `MemAvailable`)
- Uses `start_color()` + `init_pair()` to register three threshold-based color pairs:

| Load | Color | NCurses Attribute |
|---|---|---|
| `< 50%` | 🟢 Green | `COLOR_GREEN \| A_REVERSE` |
| `50% – 80%` | 🟡 Yellow | `COLOR_YELLOW \| A_REVERSE` |
| `> 80%` | 🔴 Blinking Red | `COLOR_RED \| A_REVERSE \| A_BLINK` |

Build & Run

```bash
g++ task3_color_alerts.cpp -o task3_color_alerts -lncurses
./task3_color_alerts
# Press 'q' to quit
```

Sample Output

```
=== Dynamic Visual Color Alert Monitor ===

CPU    [████████████████████░░░░░░░░░░░░░░░░░░░]  48.3%
RAM    [████████████████████████████░░░░░░░░░░░]  71.2%
```

---

📝 Task 4 — Historical Logging System

**File:** `task4_logger.cpp`

An automated file engine that appends timestamped CPU and RAM metrics to a local `sys_report.log` CSV file every **5 seconds**.

How it works

- Writes a CSV header on first launch (skips if file already exists and is non-empty)
- Every 5 seconds samples CPU (500 ms window) and RAM, then `std::ofstream` appends one row
- Echoes every row to stdout as a live table
- Handles `SIGINT` (Ctrl+C) gracefully — never writes a partial row

CSV Schema

```csv
timestamp,cpu_pct,ram_used_kb,ram_total_kb,ram_pct
2025-08-14 13:45:00,12.4,3145728,8388608,37.5
2025-08-14 13:45:05,18.1,3211264,8388608,38.3
...
```

Build & Run

```bash
g++ task4_logger.cpp -o task4_logger
./task4_logger
# Logs to ./sys_report.log
# Press Ctrl+C to stop
``Sample stdout

```
timestamp               | cpu%  | ram_used_kb | ram_total_kb | ram%
--------------------------------------------------------------------
2025-08-14 13:45:00     |  12.4 |     3145728 |      8388608 | 37.5
2025-08-14 13:45:05     |  18.1 |     3211264 |      8388608 | 38.3
```

---

⏱️ Task 5 — Context Switch & Uptime Monitor

**File:** `task5_uptime_ctx.cpp`

Parses raw scheduler analytics directly from the kernel, computing context switch rates and formatting system uptime into a human-readable `Days:HH:MM:SS` string.

### How it works

**Uptime** — reads `/proc/uptime` (a float of total seconds since boot) and converts via integer arithmetic:

```
raw_seconds → days / hours / minutes / seconds
```

**Context Switches** — reads the `ctxt` key from `/proc/stat` (cumulative total since boot). Differences two samples 2 seconds apart to compute:

- `ctx/s` — raw context switch rate
- `ctx/s/CPU` — normalized per logical CPU (read from `/proc/cpuinfo`)

### Build & Run

```bash
g++ task5_uptime_ctx.cpp -o task5_uptime_ctx
./task5_uptime_ctx
# Press Ctrl+C to exit
```

Sample Output

```
=== Context Switch & Uptime Monitor ===
Logical CPUs: 4
Refresh     : every 2 s

============================================================
Uptime                 Ctx Switches (total) Ctx/s (rate)  Ctx/s/CPU
------------------------------------------------------------
02:14:37               48392810             12450         3112
02:14:39               48418640             12915         3228
```

---

🛠️ Build All at Once

```bash
g++ task3_color_alerts.cpp -o task3_color_alerts -lncurses
g++ task4_logger.cpp       -o task4_logger
g++ task5_uptime_ctx.cpp   -o task5_uptime_ctx
```

Or with a simple Makefile:

```makefile
CXX     = g++
CXXFLAGS = -std=c++17 -Wall -O2

all: task3_color_alerts task4_logger task5_uptime_ctx

task3_color_alerts: task3_color_alerts.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -lncurses

task4_logger: task4_logger.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

task5_uptime_ctx: task5_uptime_ctx.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f task3_color_alerts task4_logger task5_uptime_ctx
```

```bash
make        # build all
make clean  # remove binaries
```

---

📂 Data Sources

| File | Data Extracted |
|---|---|
| `/proc/stat` | CPU time counters, cumulative context switches (`ctxt`) |
| `/proc/meminfo` | `MemTotal`, `MemAvailable` |
| `/proc/uptime` | Total seconds since boot |
| `/proc/cpuinfo` | Logical CPU count |

---

🔑 Key C++ Concepts Used

- **NCurses** — `start_color()`, `init_pair()`, `attron()` / `attroff()`, `A_BLINK`, `A_REVERSE`
- **File I/O** — `std::ifstream` for `/proc` reads, `std::ofstream` with `std::ios::app` for CSV logging
- **Signal handling** — `signal(SIGINT, handler)` for graceful shutdown
- **Time formatting** — integer arithmetic on raw seconds → Days:HH:MM:SS
- **Delta sampling** — two-snapshot difference for CPU % and ctx/s rate

---

📄 License

MIT — free to use, modify, and distribute.

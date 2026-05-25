//Ahmad Asghar//24011504-036
#include <ncurses.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <ctime>
#include <unistd.h>

// ─────────────────────────────────────────────────────────────────────────────
// Color-pair IDs  (Task 3)
// ─────────────────────────────────────────────────────────────────────────────
constexpr int CP_GREEN       = 1;   // < 50 %
constexpr int CP_YELLOW      = 2;   // 50–80 %
constexpr int CP_RED         = 3;   // > 80 %  (blinking)
constexpr int CP_TITLE       = 4;
constexpr int CP_LABEL       = 5;
constexpr int CP_INFO        = 6;

// ─────────────────────────────────────────────────────────────────────────────
// Structures
// ─────────────────────────────────────────────────────────────────────────────
struct CpuTimes {
    long long user{}, nice{}, system{}, idle{},
              iowait{}, irq{}, softirq{}, steal{};
};

struct SystemSnapshot {
    double   cpuPercent   = 0.0;
    double   ramPercent   = 0.0;
    long long ramUsedMB   = 0;
    long long ramTotalMB  = 0;
    long long ctxSwitches = 0;   // cumulative from /proc/stat
    double   uptimeSec    = 0.0; // from /proc/uptime
};

// ─────────────────────────────────────────────────────────────────────────────
// /proc readers
// ─────────────────────────────────────────────────────────────────────────────
static CpuTimes readCpuTimes()
{
    CpuTimes t;
    std::ifstream f("/proc/stat");
    std::string tag;
    f >> tag >> t.user >> t.nice >> t.system >> t.idle
             >> t.iowait >> t.irq >> t.softirq >> t.steal;
    return t;
}

static double calcCpuUsage(const CpuTimes& a, const CpuTimes& b)
{
    auto idleDelta  = (b.idle + b.iowait)  - (a.idle + a.iowait);
    auto totalA     = a.user + a.nice + a.system + a.idle
                    + a.iowait + a.irq + a.softirq + a.steal;
    auto totalB     = b.user + b.nice + b.system + b.idle
                    + b.iowait + b.irq + b.softirq + b.steal;
    auto totalDelta = totalB - totalA;
    if (totalDelta == 0) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
}

static void readMemInfo(long long& usedMB, long long& totalMB)
{
    std::ifstream f("/proc/meminfo");
    std::string key, unit;
    long long val;
    long long memTotal = 0, memFree = 0, buffers = 0, cached = 0, sReclaimable = 0;
    while (f >> key >> val >> unit) {
        if      (key == "MemTotal:")      memTotal      = val;
        else if (key == "MemFree:")       memFree       = val;
        else if (key == "Buffers:")       buffers       = val;
        else if (key == "Cached:")        cached        = val;
        else if (key == "SReclaimable:")  sReclaimable  = val;
    }
    totalMB = memTotal / 1024;
    long long usedKB = memTotal - memFree - buffers - cached - sReclaimable;
    usedMB = usedKB / 1024;
}

// Task 5 – context switches from /proc/stat
static long long readContextSwitches()
{
    std::ifstream f("/proc/stat");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("ctxt", 0) == 0) {
            std::istringstream ss(line);
            std::string key; long long val;
            ss >> key >> val;
            return val;
        }
    }
    return 0;
}

// Task 5 – uptime from /proc/uptime
static double readUptime()
{
    std::ifstream f("/proc/uptime");
    double up = 0.0;
    f >> up;
    return up;
}

// Task 5 – format seconds → "Xd Xh Xm Xs"
static std::string formatUptime(double seconds)
{
    long long s = static_cast<long long>(seconds);
    long long days    = s / 86400; s %= 86400;
    long long hours   = s / 3600;  s %= 3600;
    long long minutes = s / 60;    s %= 60;
    std::ostringstream oss;
    oss << days << "d " << std::setw(2) << std::setfill('0') << hours   << "h "
        << std::setw(2) << std::setfill('0') << minutes << "m "
        << std::setw(2) << std::setfill('0') << s       << "s";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 4 – CSV logging
// ─────────────────────────────────────────────────────────────────────────────
static const char* LOG_FILE = "sys_report.log";

static void ensureLogHeader()
{
    std::ifstream test(LOG_FILE);
    if (!test.good()) {
        std::ofstream f(LOG_FILE);
        f << "timestamp,cpu_pct,ram_pct,ram_used_mb,ram_total_mb,"
             "ctx_switches,uptime_sec\n";
    }
}

static void appendLog(const SystemSnapshot& snap)
{
    std::ofstream f(LOG_FILE, std::ios::app);
    // ISO-8601 timestamp
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

    f << std::fixed << std::setprecision(2)
      << buf              << ','
      << snap.cpuPercent  << ','
      << snap.ramPercent  << ','
      << snap.ramUsedMB   << ','
      << snap.ramTotalMB  << ','
      << snap.ctxSwitches << ','
      << snap.uptimeSec   << '\n';
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 3 – NCurses bar drawing
// ─────────────────────────────────────────────────────────────────────────────
static void drawBar(int row, int col, int barWidth, double pct,
                    const std::string& label)
{
    // Choose color + optional blink attribute
    int colorPair;
    attr_t blink = 0;
    if (pct < 50.0)       { colorPair = CP_GREEN;  }
    else if (pct < 80.0)  { colorPair = CP_YELLOW; }
    else                  { colorPair = CP_RED; blink = A_BLINK; }

    // Label
    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(row, col, "%-6s", label.c_str());
    attroff(COLOR_PAIR(CP_LABEL));

    int labelWidth = 7;
    int filled = static_cast<int>((pct / 100.0) * barWidth);
    filled = std::max(0, std::min(filled, barWidth));

    // Filled portion
    attron(COLOR_PAIR(colorPair) | blink);
    mvprintw(row, col + labelWidth, "%s", std::string(filled, '|').c_str());
    attroff(COLOR_PAIR(colorPair) | blink);

    // Empty portion
    attron(COLOR_PAIR(CP_INFO));
    mvprintw(row, col + labelWidth + filled, "%s",
             std::string(barWidth - filled, ' ').c_str());
    attroff(COLOR_PAIR(CP_INFO));

    // Percentage text
    attron(COLOR_PAIR(colorPair) | blink);
    printw(" %5.1f%%", pct);
    attroff(COLOR_PAIR(colorPair) | blink);
}

// ─────────────────────────────────────────────────────────────────────────────
// NCurses init
// ─────────────────────────────────────────────────────────────────────────────
static void initNCurses()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);   // non-blocking getch
    curs_set(0);

    start_color();
    use_default_colors();

    // Task 3 color pairs
    init_pair(CP_GREEN,  COLOR_GREEN,  COLOR_BLACK);
    init_pair(CP_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(CP_RED,    COLOR_RED,    COLOR_BLACK);
    init_pair(CP_TITLE,  COLOR_CYAN,   COLOR_BLACK);
    init_pair(CP_LABEL,  COLOR_WHITE,  COLOR_BLACK);
    init_pair(CP_INFO,   COLOR_WHITE,  COLOR_BLACK);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw the full screen
// ─────────────────────────────────────────────────────────────────────────────
static void drawScreen(const SystemSnapshot& snap, int logCountdown)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    clear();

    // Title
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(0, 2, "╔══════════════════════════════════════════╗");
    mvprintw(1, 2, "║     SYSTEM MONITOR  –  Tasks 3 / 4 / 5  ║");
    mvprintw(2, 2, "╚══════════════════════════════════════════╝");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    int barWidth = cols - 22;   // leave room for label + percentage

    // ── Task 3 bars ──────────────────────────────────────────────────────────
    attron(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);
    mvprintw(4, 2, "RESOURCE USAGE (Task 3 – Dynamic Color Alerts)");
    attroff(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);

    drawBar(6,  2, barWidth, snap.cpuPercent, "CPU");
    drawBar(8,  2, barWidth, snap.ramPercent, "RAM");

    // Legend
    attron(COLOR_PAIR(CP_GREEN));  mvprintw(10, 2, "■ Green");  attroff(COLOR_PAIR(CP_GREEN));
    printw(" < 50%%   ");
    attron(COLOR_PAIR(CP_YELLOW)); mvprintw(10, 16, "■ Yellow"); attroff(COLOR_PAIR(CP_YELLOW));
    printw(" 50–80%%   ");
    attron(COLOR_PAIR(CP_RED) | A_BLINK); mvprintw(10, 32, "■ Red");    attroff(COLOR_PAIR(CP_RED) | A_BLINK);
    printw(" > 80%%");

    // ── Task 5 uptime & context switches ─────────────────────────────────────
    attron(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);
    mvprintw(12, 2, "SCHEDULER ANALYTICS (Task 5)");
    attroff(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);

    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(14, 2,  "Uptime       : ");
    attroff(COLOR_PAIR(CP_LABEL));
    attron(COLOR_PAIR(CP_INFO));
    printw("%s", formatUptime(snap.uptimeSec).c_str());
    attroff(COLOR_PAIR(CP_INFO));

    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(15, 2,  "Ctx Switches : ");
    attroff(COLOR_PAIR(CP_LABEL));
    attron(COLOR_PAIR(CP_INFO));
    printw("%lld (cumulative)", snap.ctxSwitches);
    attroff(COLOR_PAIR(CP_INFO));

    // ── Task 4 RAM detail + log countdown ────────────────────────────────────
    attron(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);
    mvprintw(17, 2, "MEMORY DETAIL & LOGGING (Task 4)");
    attroff(COLOR_PAIR(CP_TITLE) | A_UNDERLINE);

    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(19, 2, "RAM Used/Total: ");
    attroff(COLOR_PAIR(CP_LABEL));
    attron(COLOR_PAIR(CP_INFO));
    printw("%lld MB / %lld MB", snap.ramUsedMB, snap.ramTotalMB);
    attroff(COLOR_PAIR(CP_INFO));

    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(20, 2, "Log file      : ");
    attroff(COLOR_PAIR(CP_LABEL));
    attron(COLOR_PAIR(CP_INFO));
    printw("%s", LOG_FILE);
    attroff(COLOR_PAIR(CP_INFO));

    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(21, 2, "Next log in   : ");
    attroff(COLOR_PAIR(CP_LABEL));
    attron(logCountdown <= 1 ? (COLOR_PAIR(CP_GREEN) | A_BOLD) : COLOR_PAIR(CP_INFO));
    printw("%d s", logCountdown);
    attroff(COLOR_PAIR(CP_INFO) | A_BOLD | COLOR_PAIR(CP_GREEN));

    // Footer
    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(23, 2, "Press 'q' to quit");
    attroff(COLOR_PAIR(CP_LABEL));

    refresh();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    ensureLogHeader();
    initNCurses();

    constexpr int LOG_INTERVAL_SEC = 5;    // Task 4 – log every 5 s
    constexpr int REFRESH_MS       = 1000; // UI refresh every 1 s

    CpuTimes prev = readCpuTimes();
    int logCountdown = LOG_INTERVAL_SEC;

    while (true) {
        // Non-blocking quit check
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        // Sleep 1 second (in 50 ms slices so 'q' is responsive)
        for (int i = 0; i < REFRESH_MS / 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ch = getch();
            if (ch == 'q' || ch == 'Q') goto done;
        }

        // Gather snapshot
        SystemSnapshot snap;

        CpuTimes curr = readCpuTimes();
        snap.cpuPercent = calcCpuUsage(prev, curr);
        prev = curr;

        readMemInfo(snap.ramUsedMB, snap.ramTotalMB);
        snap.ramPercent = snap.ramTotalMB > 0
            ? 100.0 * snap.ramUsedMB / snap.ramTotalMB
            : 0.0;

        snap.ctxSwitches = readContextSwitches();   // Task 5
        snap.uptimeSec   = readUptime();            // Task 5

        // Task 4 – log every 5 seconds
        --logCountdown;
        if (logCountdown <= 0) {
            appendLog(snap);
            logCountdown = LOG_INTERVAL_SEC;
        }

        drawScreen(snap, logCountdown);
    }

done:
    endwin();
    std::cout << "\nExited. Log saved to: " << LOG_FILE << '\n';
    return 0;
}
//Ahmad Asghar//24011504-036
#include <ncurses.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <cmath>
 
// ---------- Color pair IDs ----------
#define COLOR_GREEN_BAR   1
#define COLOR_YELLOW_BAR  2
#define COLOR_RED_BAR     3
#define COLOR_TITLE       4
#define COLOR_LABEL       5
#define COLOR_BG          6
 
// ---------- CPU snapshot -----------
struct CpuSnapshot {
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    long long total()  const { return user + nice + system + idle + iowait + irq + softirq + steal; }
    long long active() const { return user + nice + system + irq + softirq + steal; }
};
 
CpuSnapshot readCpuSnapshot() {
    CpuSnapshot s{};
    std::ifstream f("/proc/stat");
    std::string label;
    f >> label >> s.user >> s.nice >> s.system >> s.idle
      >> s.iowait >> s.irq >> s.softirq >> s.steal;
    return s;
}
 
double getCpuUsage() {
    CpuSnapshot a = readCpuSnapshot();
    usleep(200'000);          // 200 ms sample window
    CpuSnapshot b = readCpuSnapshot();
 
    long long dTotal  = b.total()  - a.total();
    long long dActive = b.active() - a.active();
    if (dTotal == 0) return 0.0;
    return 100.0 * dActive / dTotal;
}
 
// ---------- RAM usage ---------------
double getRamUsage() {
    std::ifstream f("/proc/meminfo");
    std::string key;
    long long value;
    std::string unit;
 
    long long memTotal = 0, memAvailable = 0;
    while (f >> key >> value >> unit) {
        if (key == "MemTotal:")     memTotal     = value;
        if (key == "MemAvailable:") memAvailable = value;
        if (memTotal && memAvailable) break;
    }
    if (memTotal == 0) return 0.0;
    return 100.0 * (memTotal - memAvailable) / memTotal;
}
 
// ---------- Helper: pick color pair by load -----
int colorForLoad(double pct) {
    if (pct >= 80.0) return COLOR_RED_BAR;
    if (pct >= 50.0) return COLOR_YELLOW_BAR;
    return COLOR_GREEN_BAR;
}
 
// ---------- Draw one resource bar ---
// row      : starting row
// label    : e.g. "CPU"
// pct      : 0-100
// barWidth : total character width of the bar area
void drawBar(int row, const std::string& label, double pct, int barWidth) {
    int filled = static_cast<int>(std::round(pct / 100.0 * barWidth));
    filled = std::max(0, std::min(filled, barWidth));
 
    int cpair = colorForLoad(pct);
    bool blink = (pct >= 80.0);
 
    // Label column
    attron(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
    mvprintw(row, 2, "%-6s", label.c_str());
    attroff(COLOR_PAIR(COLOR_LABEL) | A_BOLD);
 
    // Opening bracket
    mvprintw(row, 9, "[");
 
    // Filled portion
    int attr = COLOR_PAIR(cpair) | A_REVERSE;
    if (blink) attr |= A_BLINK;
    attron(attr);
    for (int i = 0; i < filled; ++i)
        mvaddch(row, 10 + i, ' ');
    attroff(attr);
 
    // Empty portion
    attron(COLOR_PAIR(COLOR_BG));
    for (int i = filled; i < barWidth; ++i)
        mvaddch(row, 10 + i, '.');
    attroff(COLOR_PAIR(COLOR_BG));
 
    // Closing bracket + percentage
    mvprintw(row, 10 + barWidth, "] ");
    attron(COLOR_PAIR(cpair) | A_BOLD | (blink ? A_BLINK : 0));
    printw("%5.1f%%", pct);
    attroff(COLOR_PAIR(cpair) | A_BOLD | A_BLINK);
 
    // Threshold legend on the first bar only (cosmetic)
    if (label == "CPU") {
        attron(COLOR_PAIR(COLOR_LABEL));
        mvprintw(row, 10 + barWidth + 15, " [<50 ");
        attron(COLOR_PAIR(COLOR_GREEN_BAR)  | A_REVERSE); printw("OK");    attroff(COLOR_PAIR(COLOR_GREEN_BAR)  | A_REVERSE);
        attron(COLOR_PAIR(COLOR_LABEL));     printw(" 50-80 ");
        attron(COLOR_PAIR(COLOR_YELLOW_BAR) | A_REVERSE); printw("WARN");  attroff(COLOR_PAIR(COLOR_YELLOW_BAR) | A_REVERSE);
        attron(COLOR_PAIR(COLOR_LABEL));     printw(" >80 ");
        attron(COLOR_PAIR(COLOR_RED_BAR)    | A_REVERSE | A_BLINK); printw("CRIT"); attroff(COLOR_PAIR(COLOR_RED_BAR) | A_REVERSE | A_BLINK);
        attron(COLOR_PAIR(COLOR_LABEL));     printw("]");
        attroff(COLOR_PAIR(COLOR_LABEL));
    }
}
 
int main() {
    // ---------- NCurses init ----------
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);   // non-blocking getch
    curs_set(0);
 
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Terminal does not support colors.\n");
        return 1;
    }
 
    start_color();
    use_default_colors();
 
    init_pair(COLOR_GREEN_BAR,  COLOR_GREEN,  -1);
    init_pair(COLOR_YELLOW_BAR, COLOR_YELLOW, -1);
    init_pair(COLOR_RED_BAR,    COLOR_RED,    -1);
    init_pair(COLOR_TITLE,      COLOR_CYAN,   -1);
    init_pair(COLOR_LABEL,      COLOR_WHITE,  -1);
    init_pair(COLOR_BG,         COLOR_BLACK,  -1);
 
    const int BAR_WIDTH = 40;
 
    // ---------- Main loop -------------
    while (true) {
        double cpu = getCpuUsage();
        double ram = getRamUsage();
 
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)rows; (void)cols;
 
        erase();
 
        // Title
        attron(COLOR_PAIR(COLOR_TITLE) | A_BOLD);
        mvprintw(1, 2, "=== Dynamic Visual Color Alert Monitor ===");
        attroff(COLOR_PAIR(COLOR_TITLE) | A_BOLD);
 
        attron(COLOR_PAIR(COLOR_LABEL));
        mvprintw(2, 2, "Press 'q' to quit");
        attroff(COLOR_PAIR(COLOR_LABEL));
 
        drawBar(4, "CPU",  cpu, BAR_WIDTH);
        drawBar(6, "RAM",  ram, BAR_WIDTH);
 
        // Status line
        attron(COLOR_PAIR(COLOR_LABEL));
        mvprintw(8, 2, "Refreshing every ~1 s  |  Blink threshold: >=80%%");
        attroff(COLOR_PAIR(COLOR_LABEL));
 
        refresh();
 
        // Poll for 'q' for ~800 ms, then re-sample
        for (int i = 0; i < 8; ++i) {
            int ch = getch();
            if (ch == 'q' || ch == 'Q') {
                endwin();
                return 0;
            }
            usleep(100'000);
        }
    }
}
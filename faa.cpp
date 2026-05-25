
#include <ncurses.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

// -------------------------------
// CPU Usage Calculation
// -------------------------------
float getCPUUsage() {
    static long prevIdle = 0, prevTotal = 0;

    ifstream file("/proc/stat");
    string line;

    getline(file, line);

    string cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;

    stringstream ss(line);

    ss >> cpu >> user >> nice >> system >> idle
       >> iowait >> irq >> softirq >> steal;

    long Idle = idle + iowait;
    long NonIdle = user + nice + system + irq + softirq + steal;
    long Total = Idle + NonIdle;

    long totald = Total - prevTotal;
    long idled = Idle - prevIdle;

    prevTotal = Total;
    prevIdle = Idle;

    if (totald == 0)
        return 0;

    return (float)(totald - idled) * 100.0 / totald;
}

// -------------------------------
// Memory Usage Calculation
// -------------------------------
float getMemoryUsage() {
    ifstream file("/proc/meminfo");

    string key;
    long value;
    string unit;

    long totalMem = 0;
    long freeMem = 0;
    long availableMem = 0;

    while (file >> key >> value >> unit) {

        if (key == "MemTotal:")
            totalMem = value;

        else if (key == "MemAvailable:")
            availableMem = value;
    }

    long usedMem = totalMem - availableMem;

    return (float)usedMem * 100.0 / totalMem;
}

// -------------------------------
// Draw Dynamic Colored Bar
// -------------------------------
void drawBar(int y, int x, float percent, const char* label) {

    int barWidth = 50;
    int filled = (percent / 100.0) * barWidth;

    mvprintw(y, x, "%s: %5.1f%% ", label, percent);

    // Color Logic
    if (percent < 50) {
        attron(COLOR_PAIR(1));
    }
    else if (percent < 80) {
        attron(COLOR_PAIR(2));
    }
    else {
        attron(COLOR_PAIR(3) | A_BLINK);
    }

    // Draw Bar
    for (int i = 0; i < barWidth; i++) {

        if (i < filled)
            mvprintw(y, x + 20 + i, " ");
        else
            mvprintw(y, x + 20 + i, "-");
    }

    attroff(COLOR_PAIR(1));
    attroff(COLOR_PAIR(2));
    attroff(COLOR_PAIR(3));
    attroff(A_BLINK);
}

// -------------------------------
// Main Function
// -------------------------------
int main() {

    // Initialize NCurses
    initscr();
    noecho();
    cbreak();
    curs_set(0);

    // Enable Colors
    start_color();

    // Green
    init_pair(1, COLOR_BLACK, COLOR_GREEN);

    // Yellow
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);

    // Red
    init_pair(3, COLOR_WHITE, COLOR_RED);

    while (true) {

        clear();

        // Title
        attron(A_BOLD);
        mvprintw(1, 25, "REAL-TIME SYSTEM MONITOR");
        attroff(A_BOLD);

        // Get System Stats
        float cpu = getCPUUsage();
        float memory = getMemoryUsage();

        // Draw Bars
        drawBar(5, 5, cpu, "CPU Usage");
        drawBar(8, 5, memory, "Memory");

        // Instructions
        mvprintw(12, 5, "Press CTRL + C to exit");

        refresh();

        sleep(1);
    }

    endwin();

    return 0;
}
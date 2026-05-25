//Ahmad Asghar//24011504-036
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <unistd.h>    // usleep, sleep
#include <csignal>
 
// ---------- Graceful shutdown flag ----------
static volatile bool g_running = true;
void handleSignal(int) { g_running = false; }
 
// ---------- CPU snapshot ----------
struct CpuSnap {
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    long long total()  const { return user + nice + system + idle + iowait + irq + softirq + steal; }
    long long active() const { return user + nice + system + irq + softirq + steal; }
};
 
CpuSnap readCpuSnap() {
    CpuSnap s{};
    std::ifstream f("/proc/stat");
    std::string label;
    f >> label >> s.user >> s.nice >> s.system >> s.idle
      >> s.iowait >> s.irq >> s.softirq >> s.steal;
    return s;
}
 
// Sample CPU over 500 ms window; returns 0-100
double sampleCpu() {
    CpuSnap a = readCpuSnap();
    usleep(500'000);
    CpuSnap b = readCpuSnap();
    long long dTotal  = b.total()  - a.total();
    long long dActive = b.active() - a.active();
    if (dTotal == 0) return 0.0;
    return 100.0 * dActive / dTotal;
}
 
// ---------- RAM usage ----------
struct MemInfo {
    long long totalKB     = 0;
    long long availableKB = 0;
    long long usedKB()    const { return totalKB - availableKB; }
    double    usedPct()   const {
        if (totalKB == 0) return 0.0;
        return 100.0 * usedKB() / totalKB;
    }
};
 
MemInfo readMemInfo() {
    MemInfo m;
    std::ifstream f("/proc/meminfo");
    std::string key;
    long long val;
    std::string unit;
    while (f >> key >> val >> unit) {
        if (key == "MemTotal:")     m.totalKB     = val;
        if (key == "MemAvailable:") m.availableKB = val;
        if (m.totalKB && m.availableKB) break;
    }
    return m;
}
 
// ---------- Timestamp (ISO-8601 local) ----------
std::string nowTimestamp() {
    time_t t = time(nullptr);
    char buf[32];
    // Format: YYYY-MM-DD HH:MM:SS
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buf;
}
 
// ---------- Write CSV header if file is new ----------
void ensureHeader(const std::string& path) {
    // Check if file exists and is non-empty
    std::ifstream check(path);
    if (check.good()) {
        check.seekg(0, std::ios::end);
        if (check.tellg() > 0) return;   // already has content
    }
    std::ofstream f(path, std::ios::app);
    f << "timestamp,cpu_pct,ram_used_kb,ram_total_kb,ram_pct\n";
}
 
int main() {
    const std::string LOG_FILE    = "sys_report.log";
    const int         INTERVAL_S  = 5;
 
    // Register signal handlers for clean exit
    signal(SIGINT,  handleSignal);
    signal(SIGTERM, handleSignal);
 
    ensureHeader(LOG_FILE);
 
    std::cout << "=== Historical Logging System ===\n";
    std::cout << "Logging to: " << LOG_FILE << "\n";
    std::cout << "Interval  : " << INTERVAL_S << " seconds\n";
    std::cout << "Press Ctrl+C to stop.\n\n";
    std::cout << "timestamp               | cpu%  | ram_used_kb | ram_total_kb | ram%\n";
    std::cout << std::string(72, '-') << "\n";
 
    while (g_running) {
        std::string ts  = nowTimestamp();
        double      cpu = sampleCpu();          // consumes 500 ms
        MemInfo     mem = readMemInfo();
 
        // Append CSV row
        {
            std::ofstream f(LOG_FILE, std::ios::app);
            if (!f) {
                std::cerr << "[ERROR] Cannot open " << LOG_FILE << " for writing.\n";
            } else {
                f << ts << ","
                  << cpu << ","
                  << mem.usedKB() << ","
                  << mem.totalKB << ","
                  << mem.usedPct() << "\n";
            }
        }
 
        // Echo to stdout
        printf("%-23s | %5.1f | %11lld | %12lld | %5.1f\n",
               ts.c_str(),
               cpu,
               mem.usedKB(),
               mem.totalKB,
               mem.usedPct());
        fflush(stdout);
 
        // Sleep remaining time after the 500 ms CPU sample
        // INTERVAL_S * 1e6 - 500'000 µs already elapsed
        int remaining_us = INTERVAL_S * 1'000'000 - 500'000;
        if (remaining_us > 0 && g_running)
            usleep(static_cast<useconds_t>(remaining_us));
    }
 
    std::cout << "\n[INFO] Logger stopped. Data saved to " << LOG_FILE << "\n";
    return 0;
}
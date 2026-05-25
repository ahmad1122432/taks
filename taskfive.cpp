//Ahmad Asghar//24011504-036
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <csignal>
 
static volatile bool g_running = true;
void handleSignal(int) { g_running = false; }
 
// ---------- Uptime ----------
// /proc/uptime: "total_seconds idle_seconds"
// Returns total uptime in whole seconds, or -1 on error.
long long readUptimeSeconds() {
    std::ifstream f("/proc/uptime");
    if (!f) return -1;
    double total, idle;
    f >> total >> idle;
    return static_cast<long long>(total);
}
 
// Convert raw seconds → "DDd HH:MM:SS"
std::string formatUptime(long long totalSeconds) {
    long long days    = totalSeconds / 86400;
    long long hours   = (totalSeconds % 86400) / 3600;
    long long minutes = (totalSeconds % 3600) / 60;
    long long seconds = totalSeconds % 60;
 
    char buf[64];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%lldd %02lld:%02lld:%02lld",
                 days, hours, minutes, seconds);
    else
        snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld",
                 hours, minutes, seconds);
    return buf;
}
 
// ---------- Context switches ----------
// /proc/stat contains a line: "ctxt <total_context_switches>"
long long readCtxSwitches() {
    std::ifstream f("/proc/stat");
    if (!f) return -1;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("ctxt ", 0) == 0) {          // starts with "ctxt "
            long long val = 0;
            std::istringstream ss(line.substr(5));
            ss >> val;
            return val;
        }
    }
    return -1;
}
 
// ---------- CPU count (for context) ----------
int cpuCount() {
    int n = 0;
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("processor", 0) == 0) ++n;
    return (n == 0) ? 1 : n;
}
 
// ---------- Simple separator ----------
void separator(char c = '-', int w = 60) {
    std::cout << std::string(w, c) << "\n";
}
 
int main() {
    signal(SIGINT,  handleSignal);
    signal(SIGTERM, handleSignal);
 
    const int INTERVAL_S = 2;      // refresh every 2 seconds
    int cpus = cpuCount();
 
    std::cout << "\n=== Context Switch & Uptime Monitor ===\n";
    std::cout << "Logical CPUs: " << cpus << "\n";
    std::cout << "Refresh     : every " << INTERVAL_S << " s\n";
    std::cout << "Press Ctrl+C to exit.\n\n";
 
    long long prevCtx  = readCtxSwitches();
    time_t    prevTime = time(nullptr);
 
    // Print table header
    separator('=');
    std::cout << std::left
              << std::setw(22) << "Uptime"
              << std::setw(20) << "Ctx Switches (total)"
              << std::setw(14) << "Ctx/s (rate)"
              << "Ctx/s/CPU\n";
    separator();
 
    while (g_running) {
        sleep(INTERVAL_S);
        if (!g_running) break;
 
        long long upSec  = readUptimeSeconds();
        long long curCtx = readCtxSwitches();
        time_t    curTime = time(nullptr);
 
        double elapsed = difftime(curTime, prevTime);
        double rate    = (elapsed > 0 && curCtx >= 0 && prevCtx >= 0)
                          ? static_cast<double>(curCtx - prevCtx) / elapsed
                          : 0.0;
        double ratePerCpu = (cpus > 0) ? rate / cpus : rate;
 
        std::string uptimeStr = (upSec >= 0) ? formatUptime(upSec) : "N/A";
        std::string ctxStr    = (curCtx >= 0) ? std::to_string(curCtx) : "N/A";
 
        std::cout << std::left
                  << std::setw(22) << uptimeStr
                  << std::setw(20) << ctxStr
                  << std::setw(14) << static_cast<long long>(rate)
                  << static_cast<long long>(ratePerCpu) << "\n";
        std::cout.flush();
 
        prevCtx  = curCtx;
        prevTime = curTime;
    }
 
    separator('=');
    std::cout << "\n[INFO] Monitor exited cleanly.\n";
    return 0;
}
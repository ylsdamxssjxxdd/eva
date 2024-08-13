// cpuChecker
// 用于查询cpu利用率、内存占用率
// 使用时可直接包含此头文件
// 需要连接对应的信号槽
// 依赖qt5

#ifndef CPUCHECKER_H
#define CPUCHECKER_H

#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#endif

class cpuChecker : public QThread {
    Q_OBJECT
   public:
    // 初始化
    cpuChecker() { ; }

    ~cpuChecker() { ; }

#ifdef _WIN32
    FILETIME preidleTime;
    FILETIME prekernelTime;
    FILETIME preuserTime;
#endif

#ifdef __linux__
    long long prevIdleTime = 0;
    long long prevTotalTime = 0;
#endif

    // 多线程支持
    void run() override { chekCpu(); }

    double CalculateCPULoad() {
#ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            // 获取系统时间失败
            return -1;
        }

        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;

        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;

        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

        // Convert previous FILETIME values to ULARGE_INTEGER.
        ULARGE_INTEGER prevIdle, prevKernel, prevUser;
        prevIdle.LowPart = preidleTime.dwLowDateTime;
        prevIdle.HighPart = preidleTime.dwHighDateTime;

        prevKernel.LowPart = prekernelTime.dwLowDateTime;
        prevKernel.HighPart = prekernelTime.dwHighDateTime;

        prevUser.LowPart = preuserTime.dwLowDateTime;
        prevUser.HighPart = preuserTime.dwHighDateTime;

        // Calculate the differences between the previous and current times.
        ULARGE_INTEGER sysIdle, sysKernel, sysUser;
        sysIdle.QuadPart = idle.QuadPart - prevIdle.QuadPart;
        sysKernel.QuadPart = kernel.QuadPart - prevKernel.QuadPart;
        sysUser.QuadPart = user.QuadPart - prevUser.QuadPart;

        // Update the stored times for the next calculation.
        preidleTime = idleTime;
        prekernelTime = kernelTime;
        preuserTime = userTime;

        // Avoid division by zero.
        if (sysKernel.QuadPart + sysUser.QuadPart == 0) {
            return 0;
        }

        // Calculate the CPU load as a percentage.
        return (sysKernel.QuadPart + sysUser.QuadPart - sysIdle.QuadPart) * 100.0 / (sysKernel.QuadPart + sysUser.QuadPart);
#endif

#ifdef __linux__

#endif
    }

   signals:
    void cpu_status(double cpuload, double memload);

   public slots:
    void chekCpu() {
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem;  // 计算内存使用率
        double cpuLoad = CalculateCPULoad();                               // 计算cpu使用率
        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif

#ifdef __linux__
        // 获取内存使用情况
        std::ifstream memInfoFile("/proc/meminfo");
        std::string line;
        unsigned long totalMem = 0;
        unsigned long freeMem = 0;
        unsigned long availMem = 0;
        unsigned long buffers = 0;
        unsigned long cached = 0;

        while (std::getline(memInfoFile, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long value;
            std::string unit;

            iss >> key >> value >> unit;

            if (key == "MemTotal:") {
                totalMem = value;
            } else if (key == "MemFree:") {
                freeMem = value;
            } else if (key == "MemAvailable:") {
                availMem = value;
            } else if (key == "Buffers:") {
                buffers = value;
            } else if (key == "Cached:") {
                cached = value;
            }
        }

        unsigned long usedMem = totalMem - availMem;
        double physMemUsedPercent = (usedMem * 100.0) / totalMem;

        // 获取CPU使用情况
        std::ifstream cpuInfoFile("/proc/stat");
        std::string cpuLine;
        std::getline(cpuInfoFile, cpuLine);
        std::istringstream cpuStream(cpuLine);

        std::string cpu;
        unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
        cpuStream >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        static unsigned long prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0, prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;
        unsigned long prevTotalIdle = prevIdle + prevIowait;
        unsigned long idleTime = idle + iowait;

        unsigned long prevTotal = prevUser + prevNice + prevSystem + prevIdle + prevIowait + prevIrq + prevSoftirq + prevSteal;
        unsigned long total = user + nice + system + idleTime + irq + softirq + steal;

        unsigned long totald = total - prevTotal;
        unsigned long idled = idleTime - prevTotalIdle;

        double cpuLoad = (totald - idled) * 100.0 / totald;

        prevUser = user;
        prevNice = nice;
        prevSystem = system;
        prevIdle = idle;
        prevIowait = iowait;
        prevIrq = irq;
        prevSoftirq = softirq;
        prevSteal = steal;

        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif
    }

    void recv_cpu_reflash() { chekCpu(); }
};

#endif  // CPUCHECKER_H

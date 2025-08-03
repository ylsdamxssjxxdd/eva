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

class cpuChecker : public QObject {
    Q_OBJECT

public:
    cpuChecker() { ; }
    ~cpuChecker() { ; }

#ifdef _WIN32
    FILETIME preIdleTime;    // 用于记录上次的空闲时间
    FILETIME preKernelTime;  // 用于记录上次的内核时间
    FILETIME preUserTime;    // 用于记录上次的用户时间
#endif

#ifdef __linux__
    long long prevIdleTime = 0;   // 用于记录上次的空闲时间
    long long prevTotalTime = 0;  // 用于记录上次的总时间
#endif

    /**
     * @brief 计算当前 CPU 使用率
     * 
     * @return CPU 使用率（0-100），如果计算失败则返回 -1
     */
    double CalculateCPULoad() {
#ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;

        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            // 获取系统时间失败，返回 -1
            return -1;
        }

        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

        // 计算当前和上次时间的差值
        ULARGE_INTEGER prevIdle, prevKernel, prevUser;
        prevIdle.LowPart = preIdleTime.dwLowDateTime;
        prevIdle.HighPart = preIdleTime.dwHighDateTime;
        prevKernel.LowPart = preKernelTime.dwLowDateTime;
        prevKernel.HighPart = preKernelTime.dwHighDateTime;
        prevUser.LowPart = preUserTime.dwLowDateTime;
        prevUser.HighPart = preUserTime.dwHighDateTime;

        ULARGE_INTEGER sysIdle, sysKernel, sysUser;
        sysIdle.QuadPart = idle.QuadPart - prevIdle.QuadPart;
        sysKernel.QuadPart = kernel.QuadPart - prevKernel.QuadPart;
        sysUser.QuadPart = user.QuadPart - prevUser.QuadPart;

        // 更新存储的时间，准备下次计算
        preIdleTime = idleTime;
        preKernelTime = kernelTime;
        preUserTime = userTime;

        // 防止除零错误
        if (sysKernel.QuadPart + sysUser.QuadPart == 0) {
            return 0;
        }

        // 计算 CPU 使用率
        return (sysKernel.QuadPart + sysUser.QuadPart - sysIdle.QuadPart) * 100.0 / (sysKernel.QuadPart + sysUser.QuadPart);
#endif

#ifdef __linux__
        // Linux 系统的 CPU 计算方法暂时留空，可根据需求添加
        return 0; // 示例返回值
#endif
    }

signals:
    void cpu_status(double cpuload, double memload);  // 用于发送 CPU 和内存负载信号

public slots:
    /**
     * @brief 检查 CPU 和内存的使用情况，并发送信号
     * 
     * 会计算当前的 CPU 使用率和内存使用率
     */
    void chekCpu() {
#ifdef _WIN32
        // 获取内存使用情况
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);

        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem;  // 计算内存使用率

        // 计算 CPU 使用率
        double cpuLoad = CalculateCPULoad();

        // 发出信号
        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif

#ifdef __linux__
        // 获取内存使用情况
        std::ifstream memInfoFile("/proc/meminfo");
        std::string line;
        unsigned long totalMem = 0;
        unsigned long freeMem = 0;
        unsigned long availMem = 0;
        while (std::getline(memInfoFile, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long value;
            std::string unit;
            iss >> key >> value >> unit;
            if (key == "MemTotal:") {
                totalMem = value;
            } else if (key == "MemAvailable:") {
                availMem = value;
            }
        }
        unsigned long usedMem = totalMem - availMem;
        double physMemUsedPercent = (usedMem * 100.0) / totalMem;  // 计算内存使用率

        // 获取 CPU 使用情况
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

        // 计算 CPU 使用率
        double cpuLoad = (totald - idled) * 100.0 / totald;

        // 更新历史数据
        prevUser = user;
        prevNice = nice;
        prevSystem = system;
        prevIdle = idle;
        prevIowait = iowait;
        prevIrq = irq;
        prevSoftirq = softirq;
        prevSteal = steal;

        // 发出信号
        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif
    }

    // 重载函数，可以通过外部触发刷新 CPU 状态
    void recv_cpu_reflash() {
        chekCpu();
    }
};

#endif  // CPUCHECKER_H

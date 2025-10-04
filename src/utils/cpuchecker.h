// cpuChecker - CPU 与内存占用查询（跨平台，Qt5）
// 注意：源码统一使用 UTF-8 编码，避免中文注释出现问号
#ifndef CPUCHECKER_H
#define CPUCHECKER_H

#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#endif

class cpuChecker : public QObject
{
    Q_OBJECT

  public:
    cpuChecker()
    {
#ifdef _WIN32
        // 首次采样：记录一次系统时间，避免第一帧出现异常峰值
        ZeroMemory(&preIdleTime, sizeof(preIdleTime));
        ZeroMemory(&preKernelTime, sizeof(preKernelTime));
        ZeroMemory(&preUserTime, sizeof(preUserTime));
        hasPrevTimes = false;
#endif
    }
    ~cpuChecker() { ; }

#ifdef _WIN32
    FILETIME preIdleTime;   // 上次的空闲时间
    FILETIME preKernelTime; // 上次的内核时间
    FILETIME preUserTime;   // 上次的用户时间
    bool hasPrevTimes = false;
#endif

    /**
     * 计算当前 CPU 使用率
     * 返回：0-100；若首次采样或失败，返回 -1
     */
    double CalculateCPULoad()
    {
#ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            return -1;
        }

        if (!hasPrevTimes)
        {
            // 首次调用仅初始化基线
            preIdleTime = idleTime;
            preKernelTime = kernelTime;
            preUserTime = userTime;
            hasPrevTimes = true;
            return -1;
        }

        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

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

        // 更新基线
        preIdleTime = idleTime;
        preKernelTime = kernelTime;
        preUserTime = userTime;

        const unsigned long long active = sysKernel.QuadPart + sysUser.QuadPart;
        const unsigned long long total = active;
        if (total == 0) return 0.0;

        const double used = (active - sysIdle.QuadPart) * 100.0 / double(total);
        // 数值稳健性：限制在 [0,100]
        if (used < 0.0) return 0.0;
        if (used > 100.0) return 100.0;
        return used;
#elif __linux__
        // 读取 /proc/stat 计算 CPU 利用率（总/空闲差分）
        std::ifstream cpuInfoFile("/proc/stat");
        if (!cpuInfoFile.is_open()) return -1;
        std::string cpuLine;
        std::getline(cpuInfoFile, cpuLine);
        std::istringstream cpuStream(cpuLine);
        std::string cpu;
        unsigned long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        cpuStream >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        static unsigned long prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0, prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;
        const unsigned long prevTotalIdle = prevIdle + prevIowait;
        const unsigned long idleTime = idle + iowait;
        const unsigned long prevTotal = prevUser + prevNice + prevSystem + prevIdle + prevIowait + prevIrq + prevSoftirq + prevSteal;
        const unsigned long total = user + nice + system + idleTime + irq + softirq + steal;
        const unsigned long totald = total - prevTotal;
        const unsigned long idled = idleTime - prevTotalIdle;

        // 更新历史数据
        prevUser = user;
        prevNice = nice;
        prevSystem = system;
        prevIdle = idle;
        prevIowait = iowait;
        prevIrq = irq;
        prevSoftirq = softirq;
        prevSteal = steal;

        if (totald == 0) return -1;
        const double cpuLoad = (totald - idled) * 100.0 / double(totald);
        if (cpuLoad < 0.0) return 0.0;
        if (cpuLoad > 100.0) return 100.0;
        return cpuLoad;
#else
        return -1;
#endif
    }

  signals:
    // 发送 CPU 与物理内存使用率（单位：百分比）
    void cpu_status(double cpuload, double memload);

  public slots:
    /**
     * 检查 CPU 和内存的使用情况，并发送信号
     */
    void chekCpu()
    {
#ifdef _WIN32
        // 物理内存使用率
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (!GlobalMemoryStatusEx(&memInfo))
        {
            emit cpu_status(CalculateCPULoad(), 0.0);
            return;
        }
        const DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        const DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        const double physMemUsedPercent = totalPhysMem ? (physMemUsed * 100.0 / double(totalPhysMem)) : 0.0;

        double cpuLoad = CalculateCPULoad();
        if (cpuLoad < 0.0) cpuLoad = 0.0; // 首帧或异常时回退为 0
        emit cpu_status(cpuLoad, physMemUsedPercent);
#elif __linux__
        // 物理内存使用率（从 /proc/meminfo 读取）
        std::ifstream memInfoFile("/proc/meminfo");
        unsigned long totalMem = 0;
        unsigned long availMem = 0;
        if (memInfoFile.is_open())
        {
            std::string line;
            while (std::getline(memInfoFile, line))
            {
                std::istringstream iss(line);
                std::string key;
                unsigned long value = 0;
                std::string unit;
                iss >> key >> value >> unit;
                if (key == "MemTotal:") totalMem = value;
                else if (key == "MemAvailable:") availMem = value;
            }
        }
        const unsigned long usedMem = (totalMem > availMem) ? (totalMem - availMem) : 0;
        const double physMemUsedPercent = totalMem ? (usedMem * 100.0 / double(totalMem)) : 0.0;

        double cpuLoad = CalculateCPULoad();
        if (cpuLoad < 0.0) cpuLoad = 0.0;
        emit cpu_status(cpuLoad, physMemUsedPercent);
#else
        emit cpu_status(0.0, 0.0);
#endif
    }

    // 外部触发刷新 CPU 状态
    void recv_cpu_reflash() { chekCpu(); }
};

#endif // CPUCHECKER_H


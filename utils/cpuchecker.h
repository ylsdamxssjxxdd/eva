// cpuChecker
// 用于查询cpu利用率、内存占用率
// 使用时可直接包含此头文件
// 需要连接对应的信号槽
// 依赖qt5

#ifndef CPUCHECKER_H
#define CPUCHECKER_H

#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QElapsedTimer>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <fstream>
#include <string>
#include <unistd.h>
#endif

class cpuChecker : public QThread
{
    Q_OBJECT
public:

    // 初始化
    cpuChecker()
    {
        ;
    }

    ~cpuChecker()
    {
        ;
    }
    
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
    void run() override
    {
        while (true) {
            chekCpu();
            QThread::msleep(500); // 500毫秒监视一次
        }
    }

    double CalculateCPULoad()
    {
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
        std::ifstream file("/proc/stat");
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string cpu;
            long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
            
            long long idleTime = idle + iowait;
            long long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
            
            long long totalDelta = totalTime - prevTotalTime;
            long long idleDelta = idleTime - prevIdleTime;

            prevTotalTime = totalTime;
            prevIdleTime = idleTime;

            if (totalDelta == 0) {
                return 0.0;
            }

            return (1.0 - (idleDelta * 1.0 / totalDelta)) * 100.0;
        }
        return -1.0;
#endif
    }

signals:
    void cpu_status(double cpuload, double memload);

public slots:
    void chekCpu()
    {
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem; // 计算内存使用率
        double cpuLoad = CalculateCPULoad(); // 计算cpu使用率
        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif       

#ifdef __linux__
        struct sysinfo memInfo;
        sysinfo(&memInfo);
        long long totalPhysMem = memInfo.totalram;
        totalPhysMem *= memInfo.mem_unit;
        long long physMemUsed = memInfo.totalram - memInfo.freeram;
        physMemUsed *= memInfo.mem_unit;
        double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem; // 计算内存使用率
        double cpuLoad = CalculateCPULoad(); // 计算cpu使用率
        emit cpu_status(cpuLoad, physMemUsedPercent);
#endif
    }
    void recv_cpu_reflash()
    {
        chekCpu();
    }
};

#endif // CPUCHECKER_H

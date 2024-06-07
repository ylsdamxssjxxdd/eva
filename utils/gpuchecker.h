// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 使用时可直接包含此头文件，windows平台依赖nvml.h和nvml.lib
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QElapsedTimer>
#include "nvml.h"

//llama模型类
class gpuChecker : public QThread
{
    Q_OBJECT
public:
    nvmlDevice_t device;//显卡设备
    nvmlUtilization_t utilization;//利用率
    nvmlMemory_t memory;

    // 初始化
    gpuChecker()
    {
        nvmlInit();// 初始化NVML库
        nvmlDeviceGetHandleByIndex(0, &device);// 获取第一个GPU的句柄
    }

    ~gpuChecker()
    {
        nvmlShutdown();
    }

    // 多线程支持
    void run() override
    {
        chekGpu();
    } 

signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree_);

public slots:
    void chekGpu()
    {
        nvmlDeviceGetUtilizationRates(device, &utilization);// 获取GPU利用率
        nvmlDeviceGetMemoryInfo(device, &memory);
        float vmem = memory.total / 1024 / 1024;//总显存MB
        float vram = float(memory.used) / float(memory.total) * 100.0;//gpu显存占用率
        float vcore = utilization.gpu;//gpu核心利用率
        float vfree_ = float(memory.free)/ 1024.0 / 1024.0;//剩余显存MB
        emit gpu_status(vmem, vram, vcore,vfree_);
    }
};

#endif // GPUCHECKER_H


    // //vulkan获取gpu总内存
    // #include <vulkan/vulkan.h>

    // void printDeviceInfo(VkPhysicalDevice device) {
    //     VkPhysicalDeviceProperties deviceProperties;
    //     vkGetPhysicalDeviceProperties(device, &deviceProperties);

    //     qDebug() << "Device Name: " << deviceProperties.deviceName;
    //     qDebug() << "Driver Version: " << deviceProperties.driverVersion;
    //     qDebug() << "API Version: " << deviceProperties.apiVersion;
    // }
    // // 初始化Vulkan
    // VkApplicationInfo appInfo = {};
    // appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    // appInfo.pApplicationName = "Hello Vulkan";
    // appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    // appInfo.pEngineName = "No Engine";
    // appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // appInfo.apiVersion = VK_API_VERSION_1_0;

    // VkInstanceCreateInfo createInfo = {};
    // createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // createInfo.pApplicationInfo = &appInfo;

    // VkInstance instance;
    // vkCreateInstance(&createInfo, nullptr, &instance);

    // // 枚举物理设备
    // uint32_t deviceCount = 0;
    // vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    // if (deviceCount == 0) {
    //     qDebug() << "Failed to find GPUs with Vulkan support!";
    //     //return;
    // }

    // std::vector<VkPhysicalDevice> devices(deviceCount);
    // vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // for (const auto& device : devices) {
    //     VkPhysicalDeviceMemoryProperties memProperties;
    //     vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

    //     for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
    //         if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
    //             qDebug() << "GPU Memory Heap " << i << ": " 
    //                       << memProperties.memoryHeaps[i].size / 1024 / 1024 << " MB";
    //         }
    //     }
    // }

    // // 打印设备信息
    // for (const auto& device : devices) {
    //     printDeviceInfo(device);
    // }

    // // 清理
    // vkDestroyInstance(instance, nullptr);

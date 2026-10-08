#include <vulkan/vulkan_raii.hpp>

#include <iostream>

class ComputeApplication
{
public:
    void run()
    {
        initVulkan();
    }

private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    void initVulkan()
    {
        createInstance();
    }

    void createInstance()
    {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "01-instance";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
        std::cout << "Available instance extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << "  " << extension.extensionName << '\n';
        }

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;

        instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
        std::cout << "Instance created.\n";
    }
};

int main()
{
    {
        ComputeApplication app;
        app.run();
    } // C API: vkDestroyInstance (app.instance 소멸자)

    std::cout << "Instance destroyed.\n";
    return 0;
}

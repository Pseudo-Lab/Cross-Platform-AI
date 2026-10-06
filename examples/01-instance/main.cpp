#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <iostream>

class ComputeApplication
{
public:
    bool run()
    {
        return initVulkan();
    }

private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    bool initVulkan()
    {
        return createInstance();
    }

    bool createInstance()
    {
        constexpr auto requestedVersion = VK_API_VERSION_1_3;

        if (!context.getDispatcher()->vkEnumerateInstanceVersion)
        {
            std::cerr << "This example requires a Vulkan 1.3 or newer loader.\n";
            return false;
        }

        const auto loaderVersion = context.enumerateInstanceVersion(); // C API: vkEnumerateInstanceVersion
        printVersion("Vulkan loader version: ", loaderVersion);
        printVersion("Requested API version: ", requestedVersion);
        if (loaderVersion < requestedVersion)
        {
            std::cerr << "The loader does not support the requested API version.\n";
            return false;
        }

        const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
        std::cout << "Available instance extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << "  " << extension.extensionName << '\n';
        }

        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "01-instance";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = requestedVersion;

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;

        instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
        std::cout << "Instance created.\n";
        return true;
    }

    static void printVersion(const char* label, std::uint32_t version)
    {
        std::cout << label
                  << VK_API_VERSION_MAJOR(version) << '.'
                  << VK_API_VERSION_MINOR(version) << '.'
                  << VK_API_VERSION_PATCH(version) << '\n';
    }
};

int main()
{
    {
        ComputeApplication app;
        if (!app.run())
        {
            return 1;
        }
    } // C API: vkDestroyInstance (app.instance 소멸자)

    std::cout << "Instance destroyed.\n";
    return 0;
}

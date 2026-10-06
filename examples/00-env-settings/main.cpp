#include <vulkan/vulkan_raii.hpp>

#include <iostream>

int main()
{
    vk::raii::Context context;
    if (!context.getDispatcher()->vkEnumerateInstanceVersion)
    {
        std::cerr << "This example requires a Vulkan 1.3 or newer loader.\n";
        return 1;
    }

    const auto version = context.enumerateInstanceVersion(); // C API: vkEnumerateInstanceVersion
    std::cout << "Vulkan loader version: "
              << VK_API_VERSION_MAJOR(version) << '.'
              << VK_API_VERSION_MINOR(version) << '.'
              << VK_API_VERSION_PATCH(version) << '\n';
    if (version < VK_API_VERSION_1_3)
    {
        std::cerr << "This example requires a Vulkan 1.3 or newer loader.\n";
        return 1;
    }
}

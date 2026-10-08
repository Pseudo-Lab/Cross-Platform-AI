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
    // TODO: Context와 Instance 멤버를 추가합니다.

    void initVulkan()
    {
        // TODO: createInstance()를 호출합니다.
    }

    // TODO: createInstance() 함수를 추가하고 생성 정보를 채웁니다.
};

int main()
{
    ComputeApplication app;
    app.run();
    return 0;
}

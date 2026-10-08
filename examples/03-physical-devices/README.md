# 03. Physical devices와 Queue families

[이전: Validation layers](../02-validation-layers/README.md) · [다음: Logical device](../04-logical-device/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

인스턴스를 생성했다면, 이제 연산에 사용할 물리적 디바이스(Physical device)를 선택할 차례입니다. 물리적 디바이스는 Vulkan이 노출하는 GPU 등의 장치입니다. 이 튜토리얼에서는 Vulkan 1.3과 컴퓨트 명령을 지원하는 첫 번째 장치를 사용하겠습니다.

[main.cpp](main.cpp)는 02장에서 완성한 인스턴스와 Debug messenger 코드에서 시작합니다. 아래 설명을 따라 장치 선택 단계를 추가해 보겠습니다. 완성된 코드는 공개 후 `examples/completed/03-physical-devices.cpp`에서 확인할 수 있습니다.

## 코드 구조

`initVulkan()`에서 인스턴스와 messenger를 만든 뒤 `pickPhysicalDevice()`를 호출하도록 바꿀 것입니다. 이 함수는 장치 목록을 가져와 API 버전과 큐 패밀리(Queue family)를 확인하고, 조건을 만족하는 장치를 멤버에 저장합니다.

먼저 기존 헤더 목록에 다음 헤더를 추가합니다.

```cpp
#include <cstdint>
#include <optional>
#include <utility>
```

`<cstdint>`는 큐 패밀리 인덱스의 정수형, `<optional>`은 검색 결과의 유무, `<utility>`는 선택한 장치를 이동하는 데 사용합니다. `debugMessenger` 바로 아래 TODO 자리에 다음 멤버를 추가합니다.

```cpp
vk::raii::PhysicalDevice physicalDevice = nullptr;
std::uint32_t computeQueueFamilyIndex = 0;
```

선택한 장치는 `physicalDevice`에, 사용할 큐 패밀리의 배열 인덱스는 `computeQueueFamilyIndex`에 저장합니다.

## 컴퓨트 큐 패밀리 찾기

Vulkan은 작업을 큐(Queue)에 제출하는 방식으로 실행합니다. 큐 패밀리는 같은 종류의 명령을 지원하는 큐의 집합입니다. 장치마다 지원하는 명령이 다르므로 컴퓨트 명령을 받을 수 있는 큐 패밀리가 있는지 확인해야 합니다.

`private` 끝의 함수 TODO 자리에 다음 함수를 추가합니다.

```cpp
static std::optional<std::uint32_t> findComputeQueueFamily(
    const vk::raii::PhysicalDevice& candidate)
{
    const auto queueFamilies = candidate.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
    for (std::uint32_t index = 0; index < queueFamilies.size(); ++index)
    {
        const auto& family = queueFamilies[index];
        if (family.queueCount > 0 && (family.queueFlags & vk::QueueFlagBits::eCompute))
        {
            return index;
        }
    }
    return std::nullopt;
}
```

`getQueueFamilyProperties()`로 큐 패밀리 목록을 가져옵니다. 각 항목의 `queueCount`는 큐의 개수이고, `queueFlags`는 지원하는 명령 종류입니다. 비트 AND인 `&`로 `eCompute`가 포함되어 있는지 검사하고, 조건을 만족하면 그 항목의 인덱스를 반환합니다.

인덱스 0도 유효한 값이므로 찾지 못한 경우를 0으로 표시할 수는 없습니다. 그래서 반환형을 `std::optional<std::uint32_t>`로 두고, 조건에 맞는 큐 패밀리가 없을 때는 `std::nullopt`를 반환합니다.

## 물리적 디바이스 선택하기

이제 장치 목록을 가져와 적합한 장치를 선택해 보겠습니다. 앞서 작성한 함수 아래에 `pickPhysicalDevice()`를 추가합니다.

```cpp
bool pickPhysicalDevice()
{
    auto physicalDevices = instance.enumeratePhysicalDevices(); // C API: vkEnumeratePhysicalDevices
    if (physicalDevices.empty())
    {
        std::cerr << "No Vulkan physical devices found.\n";
        return false;
    }

    for (auto& candidate : physicalDevices)
    {
        const auto properties = candidate.getProperties(); // C API: vkGetPhysicalDeviceProperties
        if (properties.apiVersion < VK_API_VERSION_1_3)
        {
            continue;
        }
        const auto queueFamily = findComputeQueueFamily(candidate);
        if (!queueFamily)
        {
            continue;
        }

        physicalDevice = std::move(candidate);
        computeQueueFamilyIndex = *queueFamily;
        printDeviceInfo();
        return true;
    }
    std::cerr << "No Vulkan 1.3 physical device with a compute queue family found.\n";
    return false;
}
```

`enumeratePhysicalDevices()`는 인스턴스로 접근할 수 있는 장치 목록을 반환합니다. 목록이 비어 있으면 오류를 출력하고 `false`를 반환합니다.

반복문에서는 `getProperties()`의 `apiVersion`으로 Vulkan 1.3 지원 여부를 확인합니다. 버전을 만족하면 앞서 만든 `findComputeQueueFamily()`를 호출합니다. `if (!queueFamily)`는 인덱스가 0인지가 아니라 검색 결과가 없는지를 검사합니다.

두 조건을 통과한 장치를 `std::move()`로 멤버에 옮기고, `*queueFamily`에서 인덱스를 꺼내 저장합니다. 이후 `printDeviceInfo()`로 선택 결과를 출력합니다. 모든 후보를 확인해도 적합한 장치가 없다면 `false`를 반환합니다.

## 선택한 장치 정보 확인하기

앞에서 호출한 `printDeviceInfo()`를 같은 `private` 영역에 추가합니다. 이 함수에서는 장치 이름과 지원 기능, 연산에 사용할 수 있는 크기의 한계를 확인합니다.

```cpp
void printDeviceInfo()
{
    const auto properties = physicalDevice.getProperties(); // C API: vkGetPhysicalDeviceProperties
    const auto features = physicalDevice.getFeatures(); // C API: vkGetPhysicalDeviceFeatures
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
    const auto& limits = properties.limits;

    std::cout << "Selected physical device: " << properties.deviceName << '\n'
              << "Device type: " << vk::to_string(properties.deviceType) << '\n';
    std::cout << "Device API version: "
              << VK_API_VERSION_MAJOR(properties.apiVersion) << '.'
              << VK_API_VERSION_MINOR(properties.apiVersion) << '.'
              << VK_API_VERSION_PATCH(properties.apiVersion) << '\n';
    std::cout << "Compute queue family index: " << computeQueueFamilyIndex << '\n'
              << "Queues in selected family: " << queueFamilies[computeQueueFamilyIndex].queueCount << '\n'
              << "shaderFloat64 supported (not enabled): " << (features.shaderFloat64 ? "yes" : "no") << '\n'
              << "maxComputeWorkGroupCount: " << limits.maxComputeWorkGroupCount[0] << ", "
              << limits.maxComputeWorkGroupCount[1] << ", " << limits.maxComputeWorkGroupCount[2] << '\n'
              << "maxComputeWorkGroupSize: " << limits.maxComputeWorkGroupSize[0] << ", "
              << limits.maxComputeWorkGroupSize[1] << ", " << limits.maxComputeWorkGroupSize[2] << '\n'
              << "maxComputeWorkGroupInvocations: " << limits.maxComputeWorkGroupInvocations << '\n'
              << "maxStorageBufferRange: " << limits.maxStorageBufferRange << " bytes\n";
}
```

`getProperties()`에는 장치 이름과 API 버전, `limits`가 들어 있습니다. `getFeatures()`는 선택적 기능의 지원 여부를 알려 줍니다. 여기서 조회하는 `shaderFloat64`는 64비트 부동소수점 연산의 지원 여부이며, 조회만으로 활성화되지는 않습니다.

`maxComputeWorkGroupCount`는 한 번의 dispatch에서 제출할 수 있는 각 축의 작업 그룹 수, `maxComputeWorkGroupSize`는 그룹 안의 각 축 크기, `maxComputeWorkGroupInvocations`는 그룹 안에서 실행할 셰이더의 총개수 한계입니다. `maxStorageBufferRange`는 하나의 Storage buffer descriptor로 지정할 수 있는 접근 범위입니다. 실제 버퍼와 셰이더를 작성할 때 이 한계 안에서 크기를 정하게 됩니다.

## 초기화 순서에 연결하기

필요한 함수가 준비되었으므로 기존 `initVulkan()`을 다음과 같이 교체합니다.

```cpp
bool initVulkan()
{
    return createInstance() && setupDebugMessenger() && pickPhysicalDevice();
}
```

인스턴스와 messenger 생성에 성공한 뒤 장치를 선택합니다. 선택에 실패하면 기존 `run()`과 `main()`을 거쳐 종료 코드 1이 반환됩니다. 작성이 끝난 TODO 주석은 지워도 됩니다.

이 장에서는 장치와 큐 패밀리만 선택했습니다. 다음 장에서는 이 정보로 논리적 디바이스(Logical device)를 만들고 실제 큐를 가져오겠습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/03-physical-devices
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target physical_devices
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target physical_devices_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target physical_devices
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target physical_devices_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 선택한 장치 이름과 API 버전, 큐 패밀리 인덱스, 지원 기능과 한계가 출력됩니다. 장치 이름과 수치는 실행 환경에 따라 달라집니다. 이어서 02장에서 작성한 `tutorial.callback-check` 메시지를 확인할 수 있습니다. 시작 코드는 장치를 선택하지 않고 02장의 메시지 수신까지만 실행합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Physical devices and queue families](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html)를 번역한 학습 노트를 바탕으로 컴퓨트 실습에 맞게 각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

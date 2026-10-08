# 03. Physical devices와 Queue families

[이전: Validation layers](../02-validation-layers/README.md) · [다음: Logical device](../04-logical-device/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

인스턴스를 통해 Vulkan 라이브러리를 초기화했다면, 이제 연산에 사용할 물리적 디바이스(Physical device)를 선택할 차례입니다. 컴퓨터에 GPU가 여러 개 있을 수도 있고, 각 장치가 지원하는 기능도 다를 수 있습니다. 이 튜토리얼에서는 Vulkan 1.3을 지원하고 컴퓨트 명령을 처리할 수 있는 첫 번째 장치를 사용하겠습니다.

[main.cpp](main.cpp)를 열고, 장치 목록을 가져오는 것부터 하나씩 작성해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/03-physical-devices.cpp`에서 확인할 수 있습니다.

## 장치 선택 준비하기

02장의 `submitDebugMessage()`는 콜백이 호출되는지 확인하기 위한 함수였습니다. 이 장부터는 사용하지 않습니다. 02장 코드를 직접 이어서 작성하고 있다면 해당 함수와 `run()` 안의 호출을 지워 주세요. Validation layer와 Debug messenger는 그대로 두어 Vulkan 사용 중 발생하는 경고와 오류를 받습니다.

선택한 장치는 함수가 끝난 뒤에도 사용할 수 있어야 합니다. `private` 영역의 `debugMessenger` 바로 아래에 장치와 큐 패밀리 인덱스를 보관할 멤버를 추가합니다.

```cpp
vk::raii::PhysicalDevice physicalDevice = nullptr;
std::uint32_t computeQueueFamilyIndex = 0;
```

`physicalDevice`에는 선택한 장치를 저장합니다. `computeQueueFamilyIndex`는 그 장치에서 사용할 큐 패밀리의 번호이며, 큐 패밀리를 조사한 뒤 채우겠습니다.

이번 장에서 사용할 헤더도 파일 위쪽에 추가합니다. `<cstdint>`는 인덱스의 정수형, `<optional>`은 큐 검색 결과, `<utility>`는 장치를 멤버로 옮길 때 사용합니다.

```cpp
#include <cstdint>
#include <optional>
#include <utility>
```

## 물리적 디바이스 목록 가져오기

먼저 `private` 영역에 장치를 선택할 함수의 틀을 만듭니다. 이 함수는 장치를 선택하면 `true`, 선택하지 못하면 `false`를 반환하도록 작성하겠습니다.

```cpp
bool pickPhysicalDevice()
{
}
```

함수 안에서 `enumeratePhysicalDevices()`를 호출하면 이 인스턴스로 접근할 수 있는 물리적 디바이스 목록을 가져올 수 있습니다.

```cpp
auto physicalDevices = instance.enumeratePhysicalDevices(); // C API: vkEnumeratePhysicalDevices
```

목록에 장치가 하나도 없다면 이후의 선택 과정을 진행할 수 없습니다. 바로 아래에서 목록이 비어 있는지 확인하고, 오류 메시지와 함께 실패를 반환합니다.

```cpp
if (physicalDevices.empty())
{
    std::cerr << "No Vulkan physical devices found.\n";
    return false;
}
```

장치가 있더라도 모두 우리의 요구 조건을 만족하는 것은 아닙니다. 목록의 장치를 하나씩 확인할 수 있도록 `if`문 아래에 반복문을 추가합니다. `candidate`는 지금 검사하고 있는 장치입니다.

```cpp
for (auto& candidate : physicalDevices)
{
    // 이 장치의 API 버전과 큐 패밀리를 확인합니다.
    // 조건을 만족하면 선택한 장치를 저장합니다.
}
```

조건을 만족하는 장치를 찾으면 반복문 안에서 성공을 반환할 것입니다. 따라서 반복문을 끝까지 빠져나왔다면 선택할 장치를 찾지 못한 것입니다. 닫는 중괄호 아래, `pickPhysicalDevice()`가 끝나기 전에 다음 코드를 추가합니다.

```cpp
std::cerr << "No Vulkan 1.3 physical device with a compute queue family found.\n";
return false;
```

## API 버전 확인하기

이제 반복문 안을 채워 보겠습니다. 먼저 장치의 속성을 가져옵니다. `getProperties()`로 가져오는 정보에는 장치 이름, 장치 종류, 지원하는 API 버전 등이 들어 있습니다.

```cpp
const auto properties = candidate.getProperties(); // C API: vkGetPhysicalDeviceProperties
```

우리는 Vulkan 1.3을 사용하므로, 그보다 낮은 버전만 지원하는 장치는 건너뜁니다. 속성 조회 바로 아래에 다음 조건을 추가합니다.

```cpp
if (properties.apiVersion < VK_API_VERSION_1_3)
{
    continue;
}
```

`continue`를 만나면 이 장치에 대한 나머지 검사를 생략하고 다음 장치로 넘어갑니다. 버전 조건을 통과했다면, 이제 컴퓨트 명령을 처리할 큐 패밀리가 있는지 확인해야 합니다.

## 컴퓨트 큐 패밀리 찾기

Vulkan에서는 GPU가 처리할 명령을 큐(Queue)에 제출합니다. 큐 패밀리(Queue family)는 같은 종류의 명령을 지원하는 큐의 집합입니다. 어떤 패밀리는 그래픽과 컴퓨트 명령을 모두 지원하고, 어떤 패밀리는 전송 작업을 담당할 수 있습니다. 우리는 이 중 컴퓨트 명령을 처리할 수 있는 패밀리를 찾겠습니다.

잠시 `pickPhysicalDevice()`에서 나와, 같은 `private` 영역에 큐 패밀리를 검색할 함수를 추가합니다. 이 함수는 검사할 장치를 받아 사용할 패밀리의 인덱스를 돌려줍니다. 검색 결과가 없을 수도 있으므로 반환형은 값의 유무를 함께 표현하는 `std::optional`로 둡니다.

```cpp
static std::optional<std::uint32_t> findComputeQueueFamily(
    const vk::raii::PhysicalDevice& candidate)
{
}
```

먼저 함수 안에서 장치가 제공하는 큐 패밀리 목록을 가져옵니다.

```cpp
const auto queueFamilies = candidate.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
```

목록의 각 항목에는 해당 패밀리의 큐 개수와 지원하는 명령 종류가 들어 있습니다. 패밀리를 찾으면 배열 인덱스가 필요하므로, 인덱스로 목록을 순회합니다. 조회 코드 아래에 반복문을 추가합니다.

```cpp
for (std::uint32_t index = 0; index < queueFamilies.size(); ++index)
{
    const auto& family = queueFamilies[index];
    // 컴퓨트 명령을 처리할 수 있는지 확인합니다.
}
```

`queueCount`는 이 패밀리가 제공하는 큐 개수이고, `queueFlags`는 지원하는 명령 종류입니다. 큐가 하나 이상 있고 컴퓨트 명령을 지원한다면 사용할 수 있습니다. 반복문 안의 주석을 다음 코드로 바꿉니다.

```cpp
if (family.queueCount > 0 && (family.queueFlags & vk::QueueFlagBits::eCompute))
{
    return index;
}
```

`queueFlags & vk::QueueFlagBits::eCompute`는 지원 플래그에 컴퓨트가 포함되어 있는지 확인합니다. 조건을 만족하는 첫 번째 패밀리의 인덱스를 반환하면 검색이 끝납니다.

끝까지 찾지 못했다면 검색 결과가 없다는 뜻으로 `std::nullopt`를 반환합니다. 이 줄은 반복문이 끝난 뒤, 함수의 마지막에 추가합니다.

```cpp
return std::nullopt;
```

## 조건을 만족하는 장치 저장하기

큐 검색 함수가 완성됐으니 `pickPhysicalDevice()`의 장치 반복문으로 돌아갑니다. API 버전을 검사하는 `if`문 아래에서 방금 만든 함수를 호출합니다.

```cpp
const auto queueFamily = findComputeQueueFamily(candidate);
if (!queueFamily)
{
    continue;
}
```

검색 결과가 없으면 `if (!queueFamily)` 안으로 들어가 다음 장치로 넘어갑니다. 결과가 있다면 이 장치는 버전과 큐 조건을 모두 통과한 것입니다.

이어서 선택한 장치와 큐 패밀리 인덱스를 멤버에 저장합니다. 아래 코드는 같은 반복문 안에서 방금 작성한 `if`문 다음에 넣습니다.

```cpp
physicalDevice = std::move(candidate);
computeQueueFamilyIndex = *queueFamily;
return true;
```

`std::move(candidate)`는 목록에 있던 RAII 장치 객체를 멤버로 옮깁니다. `*queueFamily`로 검색 결과에 담긴 인덱스를 꺼내 저장한 뒤 성공을 반환합니다. 이제 이 함수는 조건에 맞는 장치 하나를 고르거나, 아무 장치도 고르지 못했다는 결과를 돌려줄 수 있습니다.

## 선택한 장치 정보 확인하기

장치를 선택했으니 어떤 장치를 골랐는지 출력해 보겠습니다. `private` 영역에 출력 함수의 틀을 추가합니다.

```cpp
void printDeviceInfo()
{
}
```

먼저 함수 안에서 선택한 장치의 속성을 가져와 이름과 종류를 출력합니다. 선택 과정에서는 후보인 `candidate`를 조사했지만, 여기서는 멤버에 저장된 `physicalDevice`를 사용합니다.

```cpp
const auto properties = physicalDevice.getProperties(); // C API: vkGetPhysicalDeviceProperties
std::cout << "Selected physical device: " << properties.deviceName << '\n'
          << "Device type: " << vk::to_string(properties.deviceType) << '\n';
```

그 아래에 지원하는 API 버전도 출력합니다. `apiVersion`에는 버전이 하나의 정수로 담겨 있으므로 매크로를 사용해 major, minor, patch를 꺼냅니다.

```cpp
std::cout << "Device API version: "
          << VK_API_VERSION_MAJOR(properties.apiVersion) << '.'
          << VK_API_VERSION_MINOR(properties.apiVersion) << '.'
          << VK_API_VERSION_PATCH(properties.apiVersion) << '\n';
```

선택한 큐 패밀리의 인덱스와 그 패밀리에 속한 큐 개수도 확인하겠습니다. 앞에서 저장한 `computeQueueFamilyIndex`로 큐 패밀리 목록의 항목을 찾습니다. 다음 코드를 버전 출력 아래에 이어 붙입니다.

```cpp
const auto queueFamilies = physicalDevice.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
std::cout << "Compute queue family index: " << computeQueueFamilyIndex << '\n'
          << "Queues in selected family: " << queueFamilies[computeQueueFamilyIndex].queueCount << '\n';
```

장치의 선택적 기능은 `getFeatures()`로 조사할 수 있습니다. 예를 들어 `shaderFloat64`를 읽으면 셰이더에서 64비트 부동소수점 연산을 지원하는지 알 수 있습니다. 다음 코드를 추가해 지원 여부를 출력합니다.

```cpp
const auto features = physicalDevice.getFeatures(); // C API: vkGetPhysicalDeviceFeatures
std::cout << "shaderFloat64 supported (not enabled): " << (features.shaderFloat64 ? "yes" : "no") << '\n';
```

여기서는 지원 여부만 조회합니다. 이 기능을 실제로 사용하려면 다음 장에서 논리적 디바이스를 만들 때 활성화를 요청해야 합니다.

마지막으로 연산에 사용할 수 있는 크기의 한계를 살펴보겠습니다. 컴퓨트 셰이더의 실행 단위들은 작업 그룹(Workgroup)으로 묶이며, 그룹 수와 그룹 안의 크기는 각각 x, y, z 세 축으로 정합니다. 장치의 `limits`에는 각 축에 허용되는 최대 크기가 들어 있습니다.

```cpp
const auto& limits = properties.limits;
std::cout << "maxComputeWorkGroupCount: " << limits.maxComputeWorkGroupCount[0] << ", "
          << limits.maxComputeWorkGroupCount[1] << ", " << limits.maxComputeWorkGroupCount[2] << '\n'
          << "maxComputeWorkGroupSize: " << limits.maxComputeWorkGroupSize[0] << ", "
          << limits.maxComputeWorkGroupSize[1] << ", " << limits.maxComputeWorkGroupSize[2] << '\n';
```

`maxComputeWorkGroupCount`는 한 번의 dispatch에서 제출할 수 있는 각 축의 그룹 수 한계입니다. `maxComputeWorkGroupSize`는 그룹 하나 안에서 각 축에 배치할 수 있는 실행 단위 수의 한계입니다.

각 축의 크기를 만족하더라도 그룹 하나의 총 실행 단위 수에는 별도의 제한이 있습니다. 이를 나타내는 `maxComputeWorkGroupInvocations`와, 스토리지 버퍼 디스크립터로 지정할 수 있는 최대 바이트 범위인 `maxStorageBufferRange`도 이어서 출력합니다.

```cpp
std::cout << "maxComputeWorkGroupInvocations: " << limits.maxComputeWorkGroupInvocations << '\n'
          << "maxStorageBufferRange: " << limits.maxStorageBufferRange << " bytes\n";
```

이 값들은 이후 버퍼 크기와 셰이더 실행 크기를 정할 때 사용합니다. 세부 정의는 [VkPhysicalDeviceLimits](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceLimits.html)에서 확인할 수 있습니다.

출력 함수도 완성됐습니다. 이제 `pickPhysicalDevice()`로 돌아가 장치와 인덱스를 저장한 직후, `return true;` 앞에 `printDeviceInfo()` 호출을 추가합니다. 반복문의 마지막 부분이 다음과 같아집니다.

```cpp
physicalDevice = std::move(candidate);
computeQueueFamilyIndex = *queueFamily;
printDeviceInfo();
return true;
```

## 초기화 순서에 연결하기

이제 작성한 장치 선택 함수를 실제 초기화 흐름에 연결하겠습니다. 기존 `initVulkan()`을 다음과 같이 바꾸면 인스턴스와 Debug messenger를 만든 뒤 장치를 선택합니다.

```cpp
bool initVulkan()
{
    return createInstance() && setupDebugMessenger() && pickPhysicalDevice();
}
```

앞 단계가 실패하면 `&&` 뒤의 함수는 실행되지 않습니다. 장치 선택까지 성공하면 `true`가 반환되고, 선택한 장치는 멤버에 남아 다음 단계에서 사용할 수 있습니다.

여기까지 작성하면 사용할 물리적 디바이스와 큐 패밀리를 선택한 것입니다. 다음 장에서는 이 정보로 논리적 디바이스를 만들고 실제 큐 핸들을 가져오겠습니다.

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

코드를 모두 작성한 뒤 실행하면 선택한 장치 이름, API 버전, 큐 패밀리 인덱스와 장치의 지원 기능·한계가 출력됩니다. 이름과 수치는 실행 환경에 따라 달라집니다. `Selected physical device:`가 출력되었다면 조건에 맞는 장치를 찾아 선택한 것입니다.

이 장에서 준비한 것은 장치 선택까지입니다. GPU에 컴퓨트 명령을 제출하는 과정은 이후 장에서 이어집니다.

---

이 문서는 [Khronos Vulkan Tutorial: Physical devices and queue families](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html)를 번역한 학습 노트를 바탕으로 컴퓨트 실습에 맞게 각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

# 02. Validation layers

[이전: Instance](../01-instance/README.md) · [다음: Physical devices](../03-physical-devices/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

Vulkan은 성능을 위해 API 사용 방법을 일일이 검사하지 않습니다. 개발 중에는 Validation layer를 활성화해 잘못된 인자나 객체 사용 순서에 관한 메시지를 받아 볼 수 있습니다.

[main.cpp](main.cpp)는 01장에서 완성한 인스턴스 생성 코드에서 시작합니다. 아래 설명을 따라 레이어를 활성화하고, 메시지를 출력할 콜백을 추가해 보겠습니다. 완성된 코드는 공개 후 `examples/completed/02-validation-layers.cpp`에서 확인할 수 있습니다.

## 코드 구조

`createInstance()`에서 사용할 레이어와 확장을 지정하고, 인스턴스를 만든 뒤 `setupDebugMessenger()`에서 콜백을 등록합니다. `run()`에서는 초기화가 끝나면 확인용 메시지를 제출합니다. 새 멤버와 함수는 `private` 안의 TODO 위치에 작성합니다.

먼저 문자열을 비교하는 데 사용할 헤더를 `#include <iostream>` 위에 추가합니다.

```cpp
#include <cstring>
```

`private` 바로 아래에는 사용할 레이어 이름을, 기존 `instance` 멤버 바로 아래에는 메시지를 받을 Debug messenger를 추가합니다.

```cpp
static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
```


```cpp
vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
```

## 레이어 지원 여부 확인하기

레이어를 요청하기 전에 시스템에서 사용할 수 있는지 확인해야 합니다. `createInstance()` 앞에 다음 함수를 추가합니다.

```cpp
bool checkValidationLayerSupport()
{
    const auto layers = context.enumerateInstanceLayerProperties(); // C API: vkEnumerateInstanceLayerProperties
    for (const auto& layer : layers)
    {
        if (std::strcmp(layer.layerName, validationLayer) == 0)
        {
            return true;
        }
    }
    std::cerr << "Required layer not found: " << validationLayer << '\n';
    return false;
}
```

`enumerateInstanceLayerProperties()`는 설치된 레이어 목록을 반환합니다. 각 이름을 `std::strcmp()`로 비교하고, 일치하는 항목이 있으면 `true`를 반환합니다. 찾지 못하면 오류를 출력하고 초기화를 중단하도록 `false`를 반환합니다.

## 메시지를 받을 콜백 작성하기

이제 메시지를 받았을 때 호출할 함수를 작성해 보겠습니다. 다음 `debugCallback()`을 클래스의 `private` 안에 추가합니다.

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
    std::cerr << "[debug "
              << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity))
              << ' ' << vk::to_string(vk::DebugUtilsMessageTypeFlagsEXT(type)) << "] "
              << (callbackData->pMessageIdName ? callbackData->pMessageIdName : "unnamed")
              << ": " << callbackData->pMessage << '\n';
    return VK_FALSE;
}
```

`severity`는 메시지의 심각도이고 `type`은 일반·검증·성능 중 어떤 종류인지 나타냅니다. `callbackData`에서 식별자와 본문을 읽어 출력합니다. Vulkan에 전달할 일반 함수 포인터와 맞추기 위해 `this`가 없는 `static` 멤버 함수로 둡니다. `VKAPI_ATTR`과 `VKAPI_CALL`은 Vulkan이 요구하는 호출 형식을 맞춥니다.

반환값 `VK_FALSE`는 해당 Vulkan 호출을 중단하지 않고 계속 진행하도록 합니다.

## Debug messenger 생성 정보 설정하기

콜백을 등록하려면 어떤 메시지를 받을지 지정해야 합니다. `private` 안에 다음 함수를 추가합니다.

```cpp
static vk::DebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo()
{
    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                               | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                           | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.setPfnUserCallback(debugCallback);
    return createInfo;
}
```

`messageSeverity`에는 경고와 오류를, `messageType`에는 일반·검증·성능 메시지를 지정했습니다. 비트 OR인 `|`로 여러 값을 함께 설정할 수 있습니다. 마지막으로 `setPfnUserCallback()`에 앞서 작성한 콜백 함수를 전달합니다.

## 인스턴스에 레이어와 확장 연결하기

기존 `createInstance()`의 반환형을 `void`에서 `bool`로 바꾸고, 함수 본문은 비운 뒤 아래 블록을 순서대로 채웁니다.

```cpp
bool createInstance()
{
    // 아래 코드를 순서대로 추가합니다.
}
```

먼저 사용할 레이어가 있는지 검사합니다.

```cpp
if (!checkValidationLayerSupport())
{
    return false;
}
```

다음으로 인스턴스 확장 목록에서 `VK_EXT_debug_utils`를 찾습니다. 이 확장은 콜백 등록과 메시지 제출에 사용됩니다.

```cpp
const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
bool debugUtilsAvailable = false;
std::cout << "Available instance extensions:\n";
for (const auto& extension : extensions)
{
    std::cout << "  " << extension.extensionName << '\n';
    if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
    {
        debugUtilsAvailable = true;
    }
}
if (!debugUtilsAvailable)
{
    std::cerr << "Required instance extension not found: VK_EXT_debug_utils\n";
    return false;
}
```

목록을 출력하는 기존 반복문 안에서 확장 이름을 함께 확인합니다. 필요한 확장이 없다면 인스턴스를 만들기 전에 `false`를 반환합니다.

이어서 01장에서 작성한 애플리케이션 정보를 넣습니다. 프로그램 이름은 이 장에 맞춰 지정합니다.

```cpp
vk::ApplicationInfo appInfo{};
appInfo.pApplicationName = "02-validation-layers";
appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_3;
```

그 아래에서 인스턴스 생성 정보에 레이어와 확장을 지정하고, 인스턴스를 생성합니다.

```cpp
const char* requiredExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
auto debugCreateInfo = makeDebugMessengerCreateInfo();
vk::InstanceCreateInfo createInfo{};
createInfo.pApplicationInfo = &appInfo;
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = &validationLayer;
createInfo.enabledExtensionCount = 1;
createInfo.ppEnabledExtensionNames = requiredExtensions;
createInfo.pNext = &debugCreateInfo;

instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
std::cout << "Instance created.\n"
          << "Validation layer enabled: " << validationLayer << '\n';
return true;
```

`enabledLayerCount`와 `ppEnabledLayerNames`는 사용할 레이어의 개수와 이름 목록입니다. 확장도 같은 방식으로 개수와 이름 목록을 전달합니다.

`pNext`에는 추가 생성 정보를 연결할 수 있습니다. 여기에 Debug messenger 설정을 연결하면 인스턴스 생성·해제 중 발생하는 메시지도 받을 수 있습니다. 생성이 성공하면 `true`를 반환합니다.

## 인스턴스 생성 후의 메시지 받기

인스턴스가 살아 있는 동안 메시지를 받을 Debug messenger도 만들어 보겠습니다. `private` 안에 다음 함수를 추가합니다.

```cpp
bool setupDebugMessenger()
{
    debugMessenger = instance.createDebugUtilsMessengerEXT(makeDebugMessengerCreateInfo()); // C API: vkCreateDebugUtilsMessengerEXT
    std::cout << "Debug messenger created.\n";
    return true;
}
```

앞서 작성한 생성 정보로 messenger를 만들고 멤버에 저장합니다. `debugMessenger`는 `instance` 뒤에 선언했으므로 프로그램을 마칠 때 messenger가 먼저 해제됩니다.

## 콜백 호출 확인하기

정상적인 코드에서는 검증 오류가 출력되지 않을 수 있습니다. 콜백이 등록되었는지 확인할 수 있도록 메시지를 직접 제출하는 함수를 추가합니다.

```cpp
void submitDebugMessage()
{
    vk::DebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.pMessageIdName = "tutorial.callback-check";
    callbackData.pMessage = "Application-injected callback check; this is not a validation error.";
    instance.submitDebugUtilsMessageEXT( // C API: vkSubmitDebugUtilsMessageEXT
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
        callbackData);
}
```

앞서 설정한 필터를 통과하도록 Warning과 General을 지정합니다. 이 메시지는 애플리케이션이 직접 제출한 확인용 메시지이며, Validation layer가 발견한 오류는 아닙니다.

마지막으로 기존 `initVulkan()`, `run()`, `main()`을 아래 코드로 각각 교체합니다. `run()`은 기존처럼 `public`에 둡니다.

```cpp
bool initVulkan()
{
    return createInstance() && setupDebugMessenger();
}
```

`&&`의 왼쪽 함수가 `false`를 반환하면 오른쪽 함수는 실행하지 않습니다. 따라서 인스턴스 생성 준비에 실패하면 messenger를 만들지 않습니다.

```cpp
bool run()
{
    if (!initVulkan())
    {
        return false;
    }
    submitDebugMessage();
    return true;
}
```


```cpp
int main()
{
    {
        ComputeApplication app;
        if (!app.run())
        {
            return 1;
        }
    }

    std::cout << "Debug messenger and instance destroyed.\n";
    return 0;
}
```

`run()`까지 전달된 실패는 종료 코드 1로 반환합니다. 성공하면 안쪽 중괄호가 끝날 때 messenger와 인스턴스가 해제됩니다. 작성이 끝난 TODO 주석은 지워도 됩니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/02-validation-layers
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target validation_layers
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target validation_layers_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target validation_layers
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target validation_layers_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 확장 목록 뒤에 다음 메시지를 확인할 수 있습니다. 시작 코드는 01장처럼 인스턴스만 생성하고 해제합니다.

```text
Instance created.
Validation layer enabled: VK_LAYER_KHRONOS_validation
Debug messenger created.
[debug Warning { General }] tutorial.callback-check: Application-injected callback check; this is not a validation error.
Debug messenger and instance destroyed.
```

---

이 문서는 [Khronos Vulkan Tutorial: Validation layers](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

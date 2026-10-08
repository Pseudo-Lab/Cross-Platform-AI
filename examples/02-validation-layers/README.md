# 02. Validation layers

[이전: Instance](../01-instance/README.md) · [다음: Physical devices](../03-physical-devices/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

Vulkan API 자체의 오류 검사는 제한적입니다. 개발 중에는 Validation layer를 활성화해 잘못된 인자나 객체 사용 순서에 관한 메시지를 받아 볼 수 있습니다. 레이어가 문제를 검사하고, 우리가 작성할 콜백은 그 메시지를 콘솔에 출력합니다.

[main.cpp](main.cpp)는 01장의 인스턴스 생성 코드에서 시작합니다. 레이어를 사용할 수 있는지 확인하는 것부터 작성해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/02-validation-layers.cpp`에서 확인할 수 있습니다.

## 사용할 레이어 이름 정하기

이 실습에서는 `VK_LAYER_KHRONOS_validation`을 사용합니다. 검색할 때와 활성화할 때 같은 이름을 쓸 수 있도록 `private` 바로 아래에 상수를 추가합니다.

```cpp
static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
```

설치된 레이어의 이름과 이 문자열을 비교하려면 `std::strcmp()`가 필요합니다. 파일 위쪽의 헤더 목록에 `<cstring>`도 추가합니다.

```cpp
#include <cstring>
```

## 레이어가 설치되어 있는지 확인하기

레이어 이름을 알고 있어도 시스템에 설치되어 있지 않으면 사용할 수 없습니다. `private` 영역에 검사 함수의 틀을 추가합니다. 레이어를 찾으면 `true`, 찾지 못하면 `false`를 반환하겠습니다.

```cpp
bool checkValidationLayerSupport()
{
}
```

먼저 이 함수 안에서 설치된 레이어 목록을 가져옵니다. 아직 인스턴스를 만들기 전이므로 `context`로 조회합니다.

```cpp
const auto layers = context.enumerateInstanceLayerProperties(); // C API: vkEnumerateInstanceLayerProperties
```

조회한 목록을 하나씩 확인할 반복문을 그 아래에 만듭니다.

```cpp
for (const auto& layer : layers)
{
    // 사용할 레이어와 이름이 같은지 확인합니다.
}
```

`layerName`은 각 레이어의 이름입니다. 반복문 안의 주석을 아래 코드로 바꿉니다. `std::strcmp()`는 두 문자열이 같으면 0을 반환합니다.

```cpp
if (std::strcmp(layer.layerName, validationLayer) == 0)
{
    return true;
}
```

일치하는 이름을 찾으면 함수가 끝납니다. 반대로 반복문을 끝까지 통과했다면 필요한 레이어가 없는 것입니다. 반복문 뒤에 오류 출력과 실패 반환을 추가합니다.

```cpp
std::cerr << "Required layer not found: " << validationLayer << '\n';
return false;
```

## 메시지를 받을 콜백 작성하기

레이어가 발견한 문제를 받으려면 Vulkan이 호출할 함수를 준비해야 합니다. `private` 영역에 다음 콜백의 틀을 추가합니다.

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
}
```

`severity`는 메시지의 심각도, `type`은 메시지 종류입니다. `callbackData`에는 식별자와 본문이 들어 있습니다. 마지막 인자는 사용자 데이터를 받는 자리이며 이 예제에서는 사용하지 않습니다.

Vulkan에 일반 함수 포인터로 전달할 수 있도록 `this`가 없는 `static` 멤버 함수로 만들었습니다. `VKAPI_ATTR`과 `VKAPI_CALL`은 Vulkan이 요구하는 호출 형식을 맞춥니다.

먼저 함수 본문에 심각도와 종류를 출력하는 코드를 넣습니다. C API의 형식을 Vulkan-Hpp 형식으로 변환하면 `vk::to_string()`으로 이름을 출력할 수 있습니다.

```cpp
std::cerr << "[debug "
          << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity))
          << ' ' << vk::to_string(vk::DebugUtilsMessageTypeFlagsEXT(type)) << "] ";
```

메시지를 구분할 식별자와 실제 내용을 이어서 출력합니다. 식별자 포인터가 비어 있으면 `"unnamed"`를 사용합니다.

```cpp
std::cerr << (callbackData->pMessageIdName ? callbackData->pMessageIdName : "unnamed")
          << ": " << callbackData->pMessage << '\n';
```

이 콜백은 메시지를 출력한 뒤 Vulkan 호출을 계속 진행하게 합니다. 함수 마지막에 `VK_FALSE`를 반환합니다.

```cpp
return VK_FALSE;
```

## 어떤 메시지를 받을지 설정하기

콜백을 만들었으니 어떤 심각도와 종류의 메시지를 전달할지 정해야 합니다. 같은 설정을 인스턴스 생성 시점과 생성 후에 사용할 수 있도록 생성 정보를 돌려주는 함수를 `private` 영역에 추가합니다.

```cpp
static vk::DebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo()
{
}
```

먼저 함수 안에 생성 정보 구조체를 만들고, 경고와 오류를 받도록 지정합니다.

```cpp
vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                           | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
```

`|`는 비트 OR 연산입니다. 이렇게 여러 플래그를 합치면 Warning이나 Error에 해당하는 메시지를 받을 수 있습니다.

그 아래에는 받을 메시지 종류를 지정합니다. 일반 메시지, API 사용 검증, 성능 관련 메시지를 모두 받겠습니다.

```cpp
createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
```

이제 이 조건에 해당하는 메시지를 처리할 함수를 연결합니다. 앞에서 작성한 `debugCallback`을 지정하고 생성 정보를 반환합니다.

```cpp
createInfo.setPfnUserCallback(debugCallback);
return createInfo;
```

## 인스턴스 생성 전에 지원 여부 검사하기

레이어 검사 함수가 준비됐으니 기존 `createInstance()`에서 사용하겠습니다. 지원하지 않을 때 실패를 돌려줄 수 있도록 반환형을 `void`에서 `bool`로 바꿉니다. 기존 본문은 남겨 두고, 맨 앞의 레이어 검사 TODO 자리에 다음 코드를 넣습니다.

```cpp
if (!checkValidationLayerSupport())
{
    return false;
}
```

레이어를 찾지 못했다면 인스턴스를 만들지 않고 종료합니다. 아래에 있는 `appInfo` 설정은 그대로 사용합니다.

콜백 등록과 메시지 제출에는 `VK_EXT_debug_utils` 확장도 필요합니다. 기존 확장 목록 조회 줄 바로 아래, 반복문 앞에 검색 결과를 저장할 변수를 추가합니다.

```cpp
bool debugUtilsAvailable = false;
```

기존 `for (const auto& extension : extensions)` 안의 TODO를 아래 검사로 바꿉니다.

```cpp
if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
{
    debugUtilsAvailable = true;
}
```

목록의 확장 이름이 필요한 이름과 일치하면 사용 가능하다고 표시합니다. 모든 확장을 확인한 뒤에도 `false`라면 진행할 수 없으므로, 반복문이 끝난 다음에 아래 조건을 넣습니다.

```cpp
if (!debugUtilsAvailable)
{
    std::cerr << "Required instance extension not found: VK_EXT_debug_utils\n";
    return false;
}
```

## 레이어와 확장을 인스턴스에 연결하기

지원 여부를 확인하는 것만으로 기능이 활성화되지는 않습니다. 이제 같은 `createInstance()` 안의 `vk::InstanceCreateInfo` 설정으로 내려가겠습니다. 기존 `createInfo.pApplicationInfo = &appInfo;` 뒤에 사용할 레이어의 개수와 이름을 추가합니다.

```cpp
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = &validationLayer;
```

확장도 이름 목록과 개수를 전달해야 합니다. 기존 `vk::InstanceCreateInfo createInfo{};` 선언 바로 앞에 배열을 추가합니다.

```cpp
const char* requiredExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
```

그리고 레이어 설정 바로 아래에 이 배열을 연결합니다.

```cpp
createInfo.enabledExtensionCount = 1;
createInfo.ppEnabledExtensionNames = requiredExtensions;
```

인스턴스를 생성하거나 해제하는 과정의 메시지도 받고 싶습니다. 이를 위해 `requiredExtensions` 배열 다음, `createInfo` 선언 전에 앞서 만든 콜백 설정을 준비합니다.

```cpp
auto debugCreateInfo = makeDebugMessengerCreateInfo();
```

인스턴스 생성 정보의 `pNext`는 추가 설정 구조체를 연결하는 자리입니다. 확장 설정 아래에 다음 줄을 추가하면 인스턴스 생성·해제 시점에도 이 콜백 설정을 사용합니다.

```cpp
createInfo.pNext = &debugCreateInfo;
```

그 아래의 `instance = vk::raii::Instance(context, createInfo);`는 그대로 사용합니다. 인스턴스 생성 바로 아래에 활성화한 레이어 이름을 출력하고, 함수 끝에 성공을 반환합니다.

```cpp
std::cout << "Validation layer enabled: " << validationLayer << '\n';
return true;
```

## 인스턴스 생성 후에도 메시지 받기

`pNext`에 연결한 설정은 인스턴스 생성·해제 때 사용됩니다. 인스턴스가 살아 있는 동안의 Vulkan 호출에서도 메시지를 받으려면 Debug messenger 객체를 따로 만들어야 합니다.

먼저 기존 `instance` 멤버 바로 아래에 messenger를 보관할 멤버를 추가합니다.

```cpp
vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
```

RAII 멤버는 선언의 역순으로 해제됩니다. 따라서 이 위치에 두면 messenger가 먼저 해제되고 인스턴스는 그 뒤에 해제됩니다.

이제 `private` 영역에 messenger를 생성할 함수의 틀을 추가합니다.

```cpp
bool setupDebugMessenger()
{
}
```

함수 안에서 앞서 만든 설정을 이용해 messenger를 생성하고 멤버에 저장합니다.

```cpp
debugMessenger = instance.createDebugUtilsMessengerEXT(makeDebugMessengerCreateInfo()); // C API: vkCreateDebugUtilsMessengerEXT
```

생성이 끝났는지 확인할 출력과 성공 반환을 그 아래에 추가합니다.

```cpp
std::cout << "Debug messenger created.\n";
return true;
```

## 콜백으로 확인용 메시지 보내기

오류가 없는 코드에서는 콜백의 출력이 보이지 않을 수 있습니다. 등록된 콜백이 호출되는지 확인할 수 있도록 메시지를 직접 제출해 보겠습니다. `private` 영역에 함수의 틀을 추가합니다.

```cpp
void submitDebugMessage()
{
}
```

먼저 함수 안에 식별자와 메시지 본문을 담습니다.

```cpp
vk::DebugUtilsMessengerCallbackDataEXT callbackData{};
callbackData.pMessageIdName = "tutorial.callback-check";
callbackData.pMessage = "Application-injected callback check; this is not a validation error.";
```

그 아래에서 메시지를 제출합니다. 앞에서 정한 필터를 통과하도록 심각도는 Warning, 종류는 General로 지정합니다.

```cpp
instance.submitDebugUtilsMessageEXT( // C API: vkSubmitDebugUtilsMessageEXT
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
    callbackData);
```

이 메시지는 Validation layer가 발견한 오류가 아니라 애플리케이션이 직접 보낸 확인용 메시지입니다. 이 장에서만 사용하고, 03장부터는 함수와 호출을 제거합니다.

## 초기화와 실행 순서 연결하기

필요한 함수를 모두 작성했으니 실행 흐름에 연결하겠습니다. 기존 `initVulkan()`을 다음과 같이 바꿉니다.

```cpp
bool initVulkan()
{
    return createInstance() && setupDebugMessenger();
}
```

`&&`의 왼쪽이 `false`면 오른쪽을 실행하지 않습니다. 레이어나 확장 검사에서 실패하면 messenger 생성 단계로 넘어가지 않습니다.

초기화가 실패했을 때 확인용 메시지도 보내지 않아야 합니다. `public` 영역에 있는 기존 `run()`의 반환형을 `bool`로 바꾸고, 본문의 `initVulkan();` 호출을 아래 조건으로 교체합니다.

```cpp
if (!initVulkan())
{
    return false;
}
```

조건문 뒤, 함수가 끝나기 전에 메시지를 제출하고 성공을 반환합니다.

```cpp
submitDebugMessage();
return true;
```

마지막으로 `main()`도 실행 결과를 받아야 합니다. 기존 안쪽 블록의 `ComputeApplication app;`는 남겨 두고, `app.run();` 한 줄을 다음 조건으로 바꿉니다.

```cpp
if (!app.run())
{
    return 1;
}
```

실패는 종료 코드 1로 전달됩니다. 성공하면 안쪽 블록을 빠져나오며 messenger와 인스턴스가 순서대로 해제됩니다. 블록 뒤의 종료 출력은 시작 코드에 준비되어 있습니다.

이제 레이어 지원 확인 → 인스턴스 생성 → messenger 생성 → 확인용 메시지 제출 순서로 실행됩니다. 다음 장부터는 이 설정으로 실제 Vulkan 사용 중 발생하는 메시지를 받으면서 물리적 디바이스를 선택하겠습니다.

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

활성화한 레이어 이름과 Debug messenger 생성 여부를 확인합니다. 직접 제출한 메시지가 콜백에 도착했다면 심각도·종류·본문이 함께 출력되어야 합니다.

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Validation layers](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

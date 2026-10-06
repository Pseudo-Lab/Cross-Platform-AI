# 02. Validation layer와 Debug messenger

[이전: Instance](../01-instance/README-kr.md) · [다음: Physical device](../03-physical-devices/README-kr.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

`VK_LAYER_KHRONOS_validation`을 활성화하고 진단 메시지를 받는 콜백을 등록합니다.
01장의 `ComputeApplication`에 레이어 확인, Debug messenger 생성, 메시지 확인 함수를 추가합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README-kr.md)을 완료합니다.
C++20, CMake 3.24 이상, Vulkan SDK 1.3 이상과 설치된 validation layer가 필요합니다.
저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/02-validation-layers
```

Windows의 Visual Studio 생성기에서는 다음 명령을 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target validation_layers
.\build\Debug\validation_layers.exe
```

Linux의 Makefiles 또는 Ninja에서는 다음 명령을 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target validation_layers
./build/validation_layers
```

각 폴더를 독립적으로 빌드합니다. 전체 코드는 [main.cpp](main.cpp)에 있습니다.

## Validation layer의 역할

Vulkan은 잘못된 API 사용을 모두 검사하지 않습니다. 호출마다 검사를 수행하면 실행 비용이
증가하기 때문입니다. 잘못된 객체나 매개변수를 전달하면 결과가 보장되지 않으며,
드라이버에 따라 동작하거나 실패하는 코드가 될 수 있습니다.

Validation layer는 애플리케이션의 Vulkan 호출을 검사하는 소프트웨어 계층입니다.
객체의 수명, 매개변수, 메모리 사용, 동기화 등의 잘못된 사용을 진단합니다.
검사 범위는 활성화한 validation 설정에 따라 달라집니다.
메시지가 없다고 프로그램의 모든 동작이 검증된 것은 아닙니다.

레이어는 GPU 기능이 아닙니다. 실행 환경에 설치되어 있어야 하며,
이 예제에서는 Vulkan SDK가 제공하는 `VK_LAYER_KHRONOS_validation`을 사용합니다.
Instance에서 활성화한 레이어는 이후 Device 관련 호출도 검사합니다.
별도의 device layer를 활성화하는 방식은 사용하지 않습니다.

이 학습 예제는 Debug와 Release 모두 레이어를 활성화합니다.
레이어가 없으면 오류를 출력하고 종료합니다.

## 실행 흐름

```text
main → run
  → initVulkan
    → createInstance
      → checkValidationLayerSupport
      → 레이어·확장을 활성화한 Instance 생성
    → setupDebugMessenger
  → submitDebugMessage
    → debugCallback에서 메시지 출력
  → app 스코프 종료: Debug messenger → Instance → Context
```

`createInstance()`와 `setupDebugMessenger()`는 순서대로 실행합니다.
직접 확인한 조건이 실패하면 `false`를 반환하고 다음 단계로 진행하지 않습니다.
Vulkan-Hpp에서 발생한 예외를 별도로 처리하는 코드는 추가하지 않습니다.

## 1. 사용할 레이어 확인

레이어의 이름을 클래스에 저장합니다.

```cpp
static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
```

`checkValidationLayerSupport()`는 사용 가능한 레이어 목록에서 같은 이름을 찾습니다.

```cpp
const auto layers = context.enumerateInstanceLayerProperties(); // C API: vkEnumerateInstanceLayerProperties
for (const auto& layer : layers)
{
    if (std::strcmp(layer.layerName, validationLayer) == 0)
    {
        return true;
    }
}
```

이 조회는 레이어를 활성화하지 않습니다. 활성화할 이름은 Instance 생성 정보에 전달합니다.

```cpp
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = &validationLayer;
```

`ppEnabledLayerNames`는 문자열 포인터 배열을 가리킵니다.
이 예제에서는 레이어 하나만 사용하므로 문자열 포인터 하나의 주소를 전달합니다.

## 2. 진단 메시지를 받을 확장 활성화

레이어가 오류를 찾는 일과 애플리케이션이 메시지를 받는 일은 구분합니다.
`VK_EXT_debug_utils`는 메시지 콜백을 등록하는 Debug messenger를 제공합니다.
이 확장만 활성화해도 validation 검사가 켜지는 것은 아닙니다.

01장의 확장 목록 조회를 유지하면서 `VK_EXT_debug_utils`가 있는지 확인합니다.
그다음 Instance 생성 정보에 확장 이름을 전달합니다.

```cpp
const char* requiredExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
createInfo.enabledExtensionCount = 1;
createInfo.ppEnabledExtensionNames = requiredExtensions;
```

이 예제에서 활성화하는 instance extension은 `VK_EXT_debug_utils` 하나입니다.
확장 목록에 다른 이름이 출력되어도 자동으로 활성화되지 않습니다.

## 3. 콜백 함수

Vulkan은 메시지가 발생하면 등록한 콜백을 호출합니다.
콜백은 일반 Vulkan 함수의 반환값을 대신하지 않으며 C++ 예외 처리와도 별개입니다.

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
```

콜백은 Vulkan C API의 함수 포인터 형식을 사용합니다.
`VKAPI_ATTR`와 `VKAPI_CALL`은 플랫폼의 호출 규약을 맞춥니다.
정적 멤버 함수에는 숨겨진 `this` 인자가 없으므로 콜백으로 전달할 수 있습니다.
마지막 인자는 등록 시 지정한 `pUserData`입니다. 이 예제에서는 사용하지 않습니다.

`severity`는 메시지의 심각도입니다.

| 심각도 | 의미 |
|---|---|
| Verbose | 로더·레이어·드라이버의 상세 진단 |
| Info | 상태나 동작을 알리는 정보 |
| Warning | 오류 가능성이 있거나 주의가 필요한 사용 |
| Error | 유효하지 않은 API 사용 등의 오류 |

`type`은 메시지의 종류이며 여러 플래그가 결합될 수 있습니다.

| 종류 | 의미 |
|---|---|
| General | 일반 진단 |
| Validation | 명세 위반이나 잘못된 사용에 관한 진단 |
| Performance | 성능에 관한 진단 |

`callbackData->pMessage`에는 메시지 본문이 있습니다.
`pMessageIdName`에는 메시지 식별자가 있을 수 있으며, validation 오류에는
`VUID-...` 형태의 유효 사용 규칙 식별자가 포함될 수 있습니다.
콜백은 이 정보와 심각도·종류를 `std::cerr`로 출력합니다.

반환값은 `VK_FALSE`로 둡니다. `VK_TRUE`는 문제를 해결했다는 뜻이 아니라,
해당 호출의 중단을 요청하는 값입니다. 일반적인 진단 콜백에서는 사용하지 않습니다.

## 4. Debug messenger 생성 정보

`makeDebugMessengerCreateInfo()`는 수신할 메시지와 콜백을 지정합니다.

```cpp
vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                           | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                       | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
createInfo.setPfnUserCallback(debugCallback);
```

여기서는 Warning과 Error만 받습니다. 일반·검증·성능 메시지는 모두 허용합니다.
두 필터 조건을 만족하는 메시지만 콜백에 전달됩니다.
`setPfnUserCallback()`은 구조체의 필드를 설정하는 Vulkan-Hpp 함수이며 Vulkan 호출은 아닙니다.

### Instance 생성·해제 중의 메시지

Debug messenger를 만들려면 Instance가 필요합니다.
따라서 Instance 생성 후 만든 messenger만으로는 그 Instance의 생성 중 메시지를 받을 수 없습니다.
이를 처리하려고 `InstanceCreateInfo::pNext`에도 콜백 생성 정보를 연결합니다.

```cpp
auto debugCreateInfo = makeDebugMessengerCreateInfo();
createInfo.pNext = &debugCreateInfo;
instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
```

`pNext`는 추가 생성 정보를 연결하는 포인터입니다.
여기에 연결한 설정은 `vkCreateInstance`와 `vkDestroyInstance` 중의 메시지를 받는 데 사용됩니다.
이 설정만으로 두 호출 사이의 모든 메시지를 받는 상시 messenger가 만들어지는 것은 아닙니다.
`debugCreateInfo`는 Instance 생성 호출이 끝날 때까지 유효해야 합니다.
콜백 함수 자체는 Instance가 해제될 때까지 호출 가능해야 합니다.

### Instance 생성 후의 메시지

클래스 멤버에 messenger를 보관합니다.

```cpp
vk::raii::Context context;
vk::raii::Instance instance = nullptr;
vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
```

`setupDebugMessenger()`에서 객체를 생성합니다.

```cpp
debugMessenger = instance.createDebugUtilsMessengerEXT(makeDebugMessengerCreateInfo()); // C API: vkCreateDebugUtilsMessengerEXT
```

객체를 멤버로 보관하므로 이 함수가 끝난 뒤에도 콜백 등록이 유지됩니다.
멤버는 선언의 역순으로 소멸합니다. 따라서 messenger의 `vkDestroyDebugUtilsMessengerEXT`가
Instance의 `vkDestroyInstance`보다 먼저 호출됩니다.
다음 장에서 추가할 Device와 그 자원도 Instance가 살아 있는 동안 해제해야 합니다.

## 5. 메시지 수신 확인

올바른 Vulkan 호출만 사용하면 validation 오류가 출력되지 않을 수 있습니다.
`submitDebugMessage()`는 잘못된 Vulkan 호출 대신 확인용 메시지를 직접 제출합니다.

```cpp
vk::DebugUtilsMessengerCallbackDataEXT callbackData{};
callbackData.pMessageIdName = "tutorial.callback-check";
callbackData.pMessage = "Application-injected callback check; this is not a validation error.";
instance.submitDebugUtilsMessageEXT( // C API: vkSubmitDebugUtilsMessageEXT
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
    callbackData);
```

Warning은 앞에서 지정한 필터를 통과시키기 위해 선택했습니다.
이 메시지는 **애플리케이션이 주입한 콜백 확인 메시지**입니다.
Validation layer가 API 오류를 발견했다는 의미가 아닙니다.
레이어의 설치 확인과 활성화는 앞의 Instance 생성 단계에서 별도로 수행합니다.

## 완료 기준

프로그램이 종료 코드 0으로 끝나고 다음 내용을 출력해야 합니다.

```text
Validation layer enabled: VK_LAYER_KHRONOS_validation
Debug messenger created.
[debug Warning { General }] tutorial.callback-check: Application-injected callback check; this is not a validation error.
Debug messenger and instance destroyed.
```

버전과 확장 목록은 실행 환경에 따라 달라집니다.
확인용 메시지 외의 Warning이나 Error가 있다면 본문과 메시지 식별자를 확인합니다.

## 문제 해결

| 문제 | 확인할 내용 |
|---|---|
| `Required layer not found` | 00장의 validation layer 설치와 검색 경로 확인 |
| `VK_EXT_debug_utils`를 찾지 못함 | Vulkan 로더·SDK 설치와 확장 목록 확인 |
| 확인용 메시지가 보이지 않음 | messenger 생성 여부, Warning·General 필터, 표준 오류 출력 확인 |
| `VUID-...` 오류 | 식별자에 해당하는 유효 사용 규칙과 메시지에 표시된 객체·호출 확인 |

## 참고 자료와 라이선스

- [Khronos Vulkan Tutorial — Validation layers](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html)
- [VK_EXT_debug_utils](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_debug_utils.html)
- [vkSubmitDebugUtilsMessageEXT](https://docs.vulkan.org/refpages/latest/refpages/source/vkSubmitDebugUtilsMessageEXT.html)

이 문서는 Khronos Vulkan Tutorial contributors의 위 Validation layers 장을
컴퓨트 튜토리얼에 맞게 한국어로 번역·각색했습니다.
원문의 창 시스템 의존성을 제외하고, 명시적 레이어 활성화와 콜백 확인 절차를 적용했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.

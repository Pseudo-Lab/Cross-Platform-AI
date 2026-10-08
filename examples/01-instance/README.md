# 01. Instance 생성과 해제

[이전: 환경 설정](../00-env-settings/README.md) · [다음: Validation layers](../02-validation-layers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

Vulkan을 사용하려면 먼저 인스턴스(Instance)를 생성해야 합니다. 인스턴스는 애플리케이션과 Vulkan 라이브러리를 연결합니다. 생성할 때 프로그램 정보와 사용할 API 버전을 전달해 보겠습니다.

[main.cpp](main.cpp)를 열고 아래 설명을 따라 코드를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/01-instance.cpp`에서 확인할 수 있습니다.

## 초기화가 시작되는 곳 살펴보기

시작 코드의 `main()`은 `ComputeApplication` 객체를 만들고 `app.run()`을 호출합니다. 클래스의 `public` 영역에 있는 `run()`을 보면 초기화 함수를 호출하고 있습니다.

```cpp
void run()
{
    initVulkan();
}
```

`initVulkan()` 안은 아직 비어 있습니다. 앞으로 Vulkan 객체를 하나씩 만들 때마다 이 함수에 초기화 단계를 연결하겠습니다. 우선 인스턴스를 생성할 준비부터 합니다.

## 인스턴스를 보관할 자리 만들기

인스턴스는 생성 함수가 끝난 뒤에도 사용할 수 있어야 합니다. `private` 바로 아래의 멤버용 TODO를 다음 코드로 바꿉니다.

```cpp
vk::raii::Context context;
vk::raii::Instance instance = nullptr;
```

`context`는 Vulkan 로더를 통해 인스턴스 생성에 필요한 함수에 접근하게 해 줍니다. `instance`는 아직 생성하지 않았으므로 `nullptr`로 초기화합니다.

이어서 `private` 영역의 함수용 TODO 자리에 인스턴스 생성 함수의 틀을 추가합니다. 이제부터 나오는 코드는 이 함수 안에 순서대로 작성합니다.

```cpp
void createInstance()
{
}
```

## 애플리케이션 정보 작성하기

드라이버에 어떤 프로그램이 Vulkan을 사용하는지 알려주겠습니다. `createInstance()` 안에 `vk::ApplicationInfo`를 만들고 프로그램의 이름과 버전을 넣습니다.

```cpp
vk::ApplicationInfo appInfo{};
appInfo.pApplicationName = "01-instance";
appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
```

`VK_MAKE_API_VERSION(0, 1, 0, 0)`은 variant 0의 버전 1.0.0을 하나의 정수로 만듭니다. 여기에 적는 버전은 우리가 만드는 프로그램의 버전입니다.

같은 구조체에 엔진 정보도 이어서 넣습니다. 별도의 엔진을 사용하지 않으므로 이름은 `"No Engine"`으로 두겠습니다.

```cpp
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
```

프로그램 버전과 별개로, 사용할 Vulkan API 버전도 지정해야 합니다. 이 튜토리얼은 Vulkan 1.3을 사용합니다.

```cpp
appInfo.apiVersion = VK_API_VERSION_1_3;
```

구조체의 종류를 나타내는 `sType`은 Vulkan-Hpp가 설정하므로 직접 적지 않습니다.

## 사용 가능한 확장 살펴보기

확장(Extension)은 Vulkan에 기능을 추가하는 방법입니다. 인스턴스를 만들기 전에 시스템에서 제공하는 인스턴스 확장을 조회해 보겠습니다. `appInfo` 설정 아래에 다음 코드를 추가합니다.

```cpp
const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
```

반환된 목록의 각 항목에는 확장 이름이 들어 있습니다. 조회 코드 아래에서 목록을 순회하며 `extensionName`을 출력합니다.

```cpp
std::cout << "Available instance extensions:\n";
for (const auto& extension : extensions)
{
    std::cout << "  " << extension.extensionName << '\n';
}
```

여기까지는 사용할 수 있는 확장을 조회한 것입니다. 실제로 확장을 사용하려면 인스턴스 생성 정보에 이름을 지정해야 합니다. 이 장은 창 없이 인스턴스만 만들므로 확장을 요청하지 않고 진행하겠습니다.

## 생성 정보에 애플리케이션 정보 연결하기

Vulkan은 객체를 만들 때 필요한 설정을 생성 정보 구조체로 받습니다. 확장 목록 반복문 아래에 `vk::InstanceCreateInfo`를 추가합니다.

```cpp
vk::InstanceCreateInfo createInfo{};
createInfo.pApplicationInfo = &appInfo;
```

`pApplicationInfo`에 앞에서 작성한 `appInfo`의 주소를 연결했습니다. 이 구조체에는 사용할 확장과 레이어의 이름 목록도 지정할 수 있습니다. 지금은 둘 다 요청하지 않으므로 개수는 기본값인 0으로 둡니다. 레이어는 다음 장에서 추가하겠습니다.

## 인스턴스 생성하고 초기화에 연결하기

생성 정보가 준비됐으니 `context`와 `createInfo`를 전달해 인스턴스를 만듭니다. 생성 정보 설정 바로 아래에 추가합니다.

```cpp
instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
std::cout << "Instance created.\n";
```

생성한 객체를 멤버인 `instance`에 저장했습니다. 생성이 성공하면 다음 줄의 메시지가 출력됩니다. 이제 `createInstance()`가 완성됐습니다.

이 함수가 실행되도록 기존 `initVulkan()`의 TODO를 호출 코드로 바꿉니다.

```cpp
void initVulkan()
{
    createInstance();
}
```

프로그램을 시작하면 `main()` → `run()` → `initVulkan()` → `createInstance()` 순서로 실행됩니다.

## 인스턴스가 해제되는 시점 확인하기

`vk::raii` 객체는 수명이 끝날 때 자신이 관리하는 Vulkan 자원을 해제합니다. 시작 코드의 `main()`에는 `app`을 감싼 안쪽 블록과 종료 출력이 준비되어 있습니다. 이 블록이 끝나면 멤버가 선언의 역순으로 소멸하므로 인스턴스가 먼저 해제되고, 그 뒤에 `Instance destroyed.`가 출력됩니다.

다음 장에서는 Vulkan 사용 중 생기는 경고와 오류를 받을 Validation layer를 추가하겠습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 작성한 `main.cpp`를 빌드해 보겠습니다. 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/01-instance
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target instance_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target instance_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target instance_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target instance_completed
./build/completed/main
```

### 실행 결과

사용 가능한 인스턴스 확장 목록이 출력되는지 확인합니다. 인스턴스 생성 뒤 해제 메시지가 나오는지도 확인해 보세요.

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Base code](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/00_Base_code.html)와 [Instance](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/01_Instance.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

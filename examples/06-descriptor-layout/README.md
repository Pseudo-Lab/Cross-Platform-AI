# 06. 디스크립터 셋 레이아웃

[이전: 버퍼와 메모리](../05-buffers-memory/README.md) · [다음: 디스크립터 풀과 셋](../07-descriptor-sets/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서 입력 두 개와 출력을 담을 버퍼를 만들었습니다. 그렇다면 셰이더는 이 버퍼들에 어떻게 접근할까요? Vulkan에서는 디스크립터(descriptor)를 통해 버퍼나 이미지 같은 리소스를 셰이더에 연결합니다.

먼저 셰이더가 사용할 리소스의 유형을 디스크립터 셋 레이아웃(descriptor set layout)으로 정의하겠습니다. 실제 버퍼를 연결하는 작업은 다음 장에서 이어집니다.

[main.cpp](main.cpp)는 이전 장의 완성 코드에서 시작합니다. 아래 설명을 따라 TODO를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/completed/06-descriptor-layout.cpp`에서 확인할 수 있습니다.

## 코드 구조

기존의 `createStorageBuffers()`까지는 버퍼를 만들고 CPU에서 데이터를 써 보는 과정입니다. 여기에 `createDescriptorSetLayout()`을 추가하겠습니다. `initVulkan()`을 다음과 같이 바꿉니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice()
        && createStorageBuffers()
        && createDescriptorSetLayout();
}
```

`private` 안에서 `BufferResource output;` 바로 아래에 레이아웃을 보관할 멤버를 추가합니다.

```cpp
vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
```

이제 클래스 끝의 TODO 위치에 함수를 추가합니다. 다음 두 절의 코드를 주석 위치에 순서대로 넣겠습니다.

```cpp
bool createDescriptorSetLayout()
{
    // 바인딩을 정의합니다.
    // 레이아웃을 생성하고 true를 반환합니다.
}
```

## 바인딩 정의하기

바인딩(binding)은 디스크립터 셋 안에서 리소스를 구분하는 번호입니다. 입력 `inputA`에 0, 입력 `inputB`에 1, 출력 `output`에 2를 사용하겠습니다. 각 번호에 어떤 디스크립터를 둘지는 `vk::DescriptorSetLayoutBinding`으로 설명합니다. 함수의 첫 번째 주석을 다음 코드로 채웁니다.

```cpp
std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};
for (std::uint32_t i = 0; i < bindings.size(); ++i)
{
    bindings[i].binding = i;
    bindings[i].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
}
```

`binding`에는 셰이더에서 사용할 번호를 넣습니다. 앞서 만든 버퍼가 스토리지 버퍼이므로 `descriptorType`은 `eStorageBuffer`입니다. `stageFlags`는 이 리소스를 어느 셰이더 단계에서 참조할지 지정하며, 여기서는 컴퓨트 셰이더만 사용합니다.

`descriptorCount`는 해당 바인딩에 들어갈 디스크립터의 개수입니다. 버퍼 안의 `float`가 여덟 개여도, 바인딩 하나가 가리키는 버퍼는 하나이므로 1로 설정합니다. `<array>`와 `<cstdint>`는 이전 장에서 이미 포함했습니다.

## 레이아웃 생성하기

이제 바인딩 배열을 하나의 레이아웃으로 묶겠습니다. 반복문 아래의 두 번째 주석을 다음 코드로 바꿉니다.

```cpp
vk::DescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
layoutInfo.pBindings = bindings.data();
descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo); // C API: vkCreateDescriptorSetLayout
std::cout << "Descriptor set layout created: 3 storage-buffer bindings.\n";
return true;
```

`bindingCount`는 바인딩 배열의 길이이고, `pBindings`는 그 배열을 가리킵니다. `vk::raii::DescriptorSetLayout` 생성자에 논리 디바이스와 생성 정보를 전달하면 레이아웃이 만들어집니다.

여기에는 아직 `inputA.buffer` 같은 버퍼 핸들이 등장하지 않습니다. 레이아웃은 각 번호에 어떤 유형의 리소스가 들어갈지 정의합니다. 다음 장에서는 이 레이아웃으로 디스크립터 셋을 할당하고 실제 버퍼를 지정하겠습니다. 파이프라인을 만들 때도 이 레이아웃을 사용하게 됩니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/06-descriptor-layout
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target descriptor_layout_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target descriptor_layout_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target descriptor_layout_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target descriptor_layout_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 버퍼 생성 출력에 이어 다음 줄이 출력됩니다. 시작 코드를 그대로 실행하면 이전 장의 버퍼 생성까지만 수행합니다.

```text
Descriptor set layout created: 3 storage-buffer bindings.
```

---

이 문서는 [Khronos Vulkan Tutorial: Descriptor layout and buffer](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

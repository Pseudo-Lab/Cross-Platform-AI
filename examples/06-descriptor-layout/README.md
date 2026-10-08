# 06. 디스크립터 셋 레이아웃

[이전: 버퍼와 메모리](../05-buffers-memory/README.md) · [다음: 디스크립터 풀과 셋](../07-descriptor-sets/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서 입력 두 개와 출력을 담을 버퍼를 만들었습니다. 그렇다면 셰이더는 이 버퍼들에 어떻게 접근할까요? Vulkan에서는 디스크립터(Descriptor)를 통해 버퍼나 이미지 같은 리소스를 셰이더에 연결합니다.

버퍼를 연결하기 전에, 셰이더가 어떤 유형의 리소스를 몇 개 사용할지 정해야 합니다. 이 구조를 정의하는 것이 디스크립터 셋 레이아웃(Descriptor set layout)입니다. 이번 장에서는 입력 두 개와 출력 하나를 위한 레이아웃을 만들겠습니다.

[main.cpp](main.cpp)를 열고, 이전 장에서 만든 버퍼 아래에 레이아웃을 추가해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/06-descriptor-layout.cpp`에서 확인할 수 있습니다.

## 레이아웃을 보관할 자리 만들기

레이아웃은 다음 장에서 디스크립터 셋을 할당할 때도 사용합니다. 함수가 끝나도 남아 있도록 `private` 영역의 `BufferResource output;` 바로 아래에 멤버를 추가합니다.

```cpp
vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
```

처음에는 빈 RAII 객체로 두고, 생성 정보를 채운 뒤 실제 레이아웃을 저장하겠습니다. 클래스 끝의 TODO 위치에는 생성 함수의 틀을 만듭니다.

```cpp
bool createDescriptorSetLayout()
{
}
```

## 바인딩 번호 정하기

디스크립터 셋 안에서 리소스를 구분하는 번호를 바인딩(Binding)이라고 합니다. 우리는 0번에 입력 `inputA`, 1번에 입력 `inputB`, 2번에 출력 `output`을 연결할 예정입니다. 셰이더에서도 같은 번호로 각각의 버퍼를 참조하게 됩니다.

먼저 함수 안에 각 바인딩의 정보를 담을 배열을 만듭니다. `vk::DescriptorSetLayoutBinding` 하나가 바인딩 하나를 설명하므로 배열의 길이는 3입니다. `<array>`와 `<cstdint>`는 이전 장에서 이미 포함했습니다.

```cpp
std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};
```

세 바인딩은 번호만 다르고 모두 스토리지 버퍼 하나를 사용합니다. 같은 설정을 반복해서 적지 않도록 배열 아래에 반복문을 추가합니다.

```cpp
for (std::uint32_t i = 0; i < bindings.size(); ++i)
{
    // 이 번호의 바인딩에 들어갈 리소스를 정의합니다.
}
```

반복문 안의 주석을 다음 코드로 바꿉니다. 배열 인덱스 `i`를 바인딩 번호로 사용하고, 이전 장에서 만든 버퍼에 맞춰 디스크립터 유형을 정합니다.

```cpp
bindings[i].binding = i;
bindings[i].descriptorType = vk::DescriptorType::eStorageBuffer;
```

이제 각 번호에 스토리지 버퍼가 들어간다는 것을 정했습니다. 이어서 같은 반복문 안에 바인딩당 디스크립터 개수를 추가합니다.

```cpp
bindings[i].descriptorCount = 1;
```

`descriptorCount`는 버퍼 안의 원소 개수가 아닙니다. 버퍼에 `float`가 여덟 개 들어 있어도, 바인딩 하나가 참조할 버퍼는 하나이므로 1입니다. 세 버퍼를 각각 다른 바인딩에 연결하므로 여기서 3을 넣지 않습니다.

마지막으로 이 바인딩에 접근할 셰이더 단계를 지정합니다. 우리는 컴퓨트 셰이더에서 버퍼를 사용할 것이므로, 개수 설정 바로 아래에 다음 줄을 추가합니다.

```cpp
bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
```

여기까지 작성하면 반복문이 0, 1, 2번 바인딩을 같은 유형·개수·셰이더 단계로 채웁니다. 입력과 출력의 용도가 달라도 모두 스토리지 버퍼 디스크립터를 사용합니다.

## 바인딩들을 레이아웃으로 묶기

바인딩 배열을 준비했으니 Vulkan에 전달할 생성 정보를 만들겠습니다. 반복문이 끝난 뒤, 함수가 끝나기 전에 다음 코드를 추가합니다.

```cpp
vk::DescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
layoutInfo.pBindings = bindings.data();
```

`bindingCount`는 배열에 들어 있는 바인딩 정보의 개수입니다. `pBindings`는 그 배열의 시작을 가리킵니다. 앞서 바인딩 하나에 설정한 `descriptorCount`와는 세는 대상이 다릅니다.

이제 논리적 디바이스와 생성 정보를 RAII 생성자에 전달합니다. 생성된 레이아웃은 앞에서 선언한 멤버에 저장합니다.

```cpp
descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo); // C API: vkCreateDescriptorSetLayout
```

레이아웃을 만들 때 바인딩 정보가 전달되므로, 지역 변수인 `bindings`와 `layoutInfo`를 계속 보관할 필요는 없습니다. 함수의 마지막에는 `bindings.size()`로 배열의 실제 바인딩 개수를 출력하고 성공을 반환합니다.

```cpp
std::cout << "Descriptor set layout created: " << bindings.size()
          << " bindings.\n";
return true;
```

여기까지의 코드에는 `inputA.buffer` 같은 실제 버퍼 핸들이 등장하지 않았습니다. 레이아웃은 각 번호에 들어갈 리소스의 유형과 개수를 정의합니다. 다음 장에서는 이 구조를 따르는 디스크립터 셋을 할당하고 각 번호에 실제 버퍼를 연결하겠습니다.

## 초기화 순서에 연결하기

함수가 완성됐으니 초기화 과정에서 호출하겠습니다. `initVulkan()`의 끝부분의 `&& createStorageBuffers();` 한 줄을 다음 두 줄로 바꿉니다. 앞서 작성한 인스턴스·장치 초기화 호출은 그대로 이어집니다.

```cpp
    && createStorageBuffers()
    && createDescriptorSetLayout();
```

버퍼 생성까지 성공하면 레이아웃 생성이 실행됩니다. `&&`로 연결했으므로 앞 단계에서 `false`를 반환하면 뒤의 호출은 실행되지 않습니다.

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

레이아웃 생성 결과에 표시된 바인딩 개수를 자신이 작성한 `bindings` 배열의 크기와 비교해 보세요. 바인딩마다 지정한 리소스 유형도 함께 확인합니다.

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Descriptor layout and buffer](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.

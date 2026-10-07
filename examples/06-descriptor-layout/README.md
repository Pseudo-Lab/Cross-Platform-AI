# 06. Descriptor set layout

[이전: Buffer와 memory](../05-buffers-memory/README.md) · [다음: Descriptor pool과 set](../07-descriptor-sets/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

입력 버퍼 두 개와 출력 버퍼 한 개에 대응하는 descriptor set layout을 만듭니다.
셰이더의 `set`, `binding`, `std430` 선언을 C++의 리소스 설정과 연결합니다.

05장의 `ComputeApplication`에 `descriptorSetLayout` 멤버와
`createDescriptorSetLayout()` 함수를 추가합니다.
전체 코드는 [main.cpp](main.cpp)에 있습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 먼저 완료합니다.
C++20, CMake 3.24 이상, Vulkan SDK와 장치의 Vulkan 1.3 지원이 필요합니다.
02장에서 설정한 validation layer도 사용합니다.

저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/06-descriptor-layout
```

Windows에서는 다음 명령을 실행합니다. 실행 경로는 Visual Studio 생성기 기준입니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target descriptor_layout_example
.\build\Debug\descriptor_layout_example.exe
```

Linux에서 Makefiles 또는 Ninja를 사용하면 다음과 같습니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target descriptor_layout_example
./build/descriptor_layout_example
```

이 폴더에서 독립적으로 구성하고 빌드합니다. 이전 장의 `build` 폴더는 재사용하지 않습니다.

## 1. Buffer에서 셰이더까지

05장에서 만든 `inputA`, `inputB`, `output`은 각각 8개의 `float`를 저장합니다.
메모리를 할당하고 값을 기록했지만, 셰이더가 사용할 버퍼를 아직 지정하지 않았습니다.
descriptor는 셰이더에서 참조할 리소스와 그 범위를 기술합니다.

다음 세 작업을 구분합니다.

| 단계 | 정하는 내용 | 예 |
|---|---|---|
| Descriptor set layout 생성 | binding별 리소스 종류·개수·사용 단계 | binding 0에 컴퓨트용 storage buffer 한 개 |
| Descriptor set 할당·갱신 | 실제로 참조할 리소스 | binding 0에 `inputA.buffer`의 32바이트 연결 |
| Command buffer에 set 바인딩 | 명령이 사용할 descriptor set | 컴퓨트 명령에 set 0 지정 |

이 장은 첫 번째 작업을 수행합니다. 실제 버퍼 연결은 07장, 명령 기록은 10장에서 수행합니다.

## 2. SSBO와 set·binding

SSBO는 Shader Storage Buffer Object의 약칭입니다. 셰이더가 버퍼 데이터를 읽거나
쓸 수 있습니다. 벡터 덧셈에서는 두 입력을 읽고 결과를 출력 버퍼에 씁니다.

08장에서 만들 GLSL 셰이더는 다음 선언을 사용합니다. 여기서는 선언의 의미만 확인합니다.

```glsl
layout(std430, set = 0, binding = 0) readonly buffer InputA {
    float values[];
} inputA;

layout(std430, set = 0, binding = 1) readonly buffer InputB {
    float values[];
} inputB;

layout(std430, set = 0, binding = 2) writeonly buffer Output {
    float values[];
} outputData;
```

`set`은 descriptor set의 번호이며, `binding`은 그 set 안의 리소스 번호입니다.
셰이더의 블록 이름이나 변수 이름이 C++ 변수 이름과 같을 필요는 없습니다.
번호와 descriptor 종류가 일치해야 합니다.

| GLSL 선언 | Set | Binding | C++에서 연결할 버퍼 | Descriptor 종류 |
|---|---:|---:|---|---|
| `inputA.values` | 0 | 0 | `inputA.buffer` | `eStorageBuffer` |
| `inputB.values` | 0 | 1 | `inputB.buffer` | `eStorageBuffer` |
| `outputData.values` | 0 | 2 | `output.buffer` | `eStorageBuffer` |

세 binding 모두 하나의 layout에 포함합니다. `DescriptorSetLayoutBinding`에는
`set` 필드가 없습니다. 09장에서 이 layout을 pipeline layout의 첫 번째 항목으로
등록하면 셰이더의 `set = 0`과 대응합니다.

`readonly`와 `writeonly`는 GLSL의 접근 제한입니다. C++의 descriptor 종류는 양쪽 모두
`eStorageBuffer`입니다. 버퍼 생성 시 지정한 `eStorageBuffer` usage도 이에 대응합니다.

## 3. std430과 데이터 정렬

`std430`은 버퍼 안의 데이터 배치를 정합니다. descriptor binding의 번호를 정하는
descriptor set layout과는 별개의 규칙입니다.

이 예제의 배열 원소는 32비트 `float`입니다. `std430`에서 정렬 단위와 배열 stride는
모두 4바이트입니다. 따라서 C++의 연속된 `float` 배열을 그대로 복사할 수 있습니다.

```text
원소 인덱스         0   1   2   3   4   5   6   7
버퍼 내 byte offset 0   4   8  12  16  20  24  28
전체 크기           8 × 4 = 32 bytes
```

| 32비트 `float` 배열 | 원소 간격 |
|---|---:|
| `std430` | 4바이트 |
| `std140` | 16바이트 |

SSBO에는 `std430`을 사용할 수 있습니다. 이 용도에 `uniformBufferStandardLayout`
기능을 켤 필요는 없습니다. 해당 기능은 uniform buffer의 배치와 관련됩니다.
[Khronos의 메모리 배치 설명](https://docs.vulkan.org/guide/latest/shader_memory_layout.html)을 참고합니다.

이 일치를 임의의 C++ 구조체에 적용하면 안 됩니다. 예를 들어 `std430`의 `vec3` 배열은
원소 간격이 16바이트입니다. C++에 `float` 세 개를 넣은 구조체가 같은 간격을 보장하지는
않습니다. 구조체·벡터·행렬을 추가할 때는 멤버 offset, 정렬, 배열 stride를 각각 맞춥니다.

이 데이터 배치 규칙과 `minStorageBufferOffsetAlignment`도 구분해야 합니다.
후자는 descriptor가 버퍼 중간을 참조할 때 시작 offset에 적용됩니다.
07장은 offset 0에서 버퍼 전체를 참조합니다.

## 4. Layout 생성 함수 추가

먼저 버퍼 멤버 뒤에 layout 멤버를 추가합니다.

```cpp
BufferResource inputA;
BufferResource inputB;
BufferResource output;
vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
```

`initVulkan()`은 앞 장의 초기화를 유지하고 마지막에 새 함수를 호출합니다.

```cpp
return createInstance()
    && setupDebugMessenger()
    && pickPhysicalDevice()
    && createLogicalDevice()
    && createStorageBuffers()
    && createDescriptorSetLayout();
```

`createDescriptorSetLayout()`에서 각 binding을 기술합니다.

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

`descriptorCount = 1`은 해당 binding에 버퍼 descriptor가 하나 있다는 뜻입니다.
버퍼에 들어 있는 `float`의 개수인 8을 넣지 않습니다. `float values[]`는 한 버퍼 내부의
데이터 배열이며 descriptor 배열이 아닙니다.

`stageFlags`는 리소스에 접근할 수 있는 셰이더 단계를 지정합니다. 셰이더 실행이나
동기화 명령은 아닙니다. 여기서는 컴퓨트 단계만 지정합니다.
[필드 정의](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutBinding.html)를 참고합니다.

이어서 binding 배열을 사용해 layout을 만듭니다.

```cpp
vk::DescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
layoutInfo.pBindings = bindings.data();

descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo); // C API: vkCreateDescriptorSetLayout
```

`bindings`와 `layoutInfo`는 생성 호출이 끝날 때까지 유효하면 됩니다.
생성된 layout은 클래스 멤버이므로 함수가 반환된 뒤에도 유지됩니다.
`app`의 스코프가 끝나면 layout의 소멸자가 `vkDestroyDescriptorSetLayout`을 호출합니다.
layout을 소유한 `device`는 그보다 나중에 해제됩니다.

## 완료 기준

앞 장의 버퍼 준비 출력 뒤에 다음 줄이 출력되고 종료 코드가 0이어야 합니다.
Validation layer가 Vulkan 사용 오류를 보고하지 않는지도 확인합니다.

```text
Descriptor set layout created: 3 storage-buffer bindings.
```

`tutorial.callback-check` 메시지는 02장에서 직접 보낸 콜백 확인 메시지입니다.
Vulkan 사용 오류와 구분합니다.

- binding 0, 1, 2가 각각 어느 버퍼에 대응할지 설명할 수 있습니다.
- 각 binding의 `descriptorCount`는 1이고 버퍼의 원소 수는 8임을 구분합니다.
- C++ `float` 배열과 GLSL `std430`의 4바이트 stride가 일치함을 설명할 수 있습니다.

이 장에서는 셰이더를 컴파일하거나 실행하지 않습니다. 따라서 layout 생성 성공만으로
셰이더 선언과 실제 코드의 일치 여부까지 검증된 것은 아닙니다.

## 참고 자료

- [Khronos Tutorial: Descriptor layout and buffer](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html)
- [Khronos Guide: Shader memory layout](https://docs.vulkan.org/guide/latest/shader_memory_layout.html)
- [VkDescriptorSetLayoutBinding](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutBinding.html)
- [VkDescriptorSetLayoutCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutCreateInfo.html)

Khronos Vulkan Tutorial contributors의 [공식 원문](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html)을
번역한 학습 노트를 바탕으로 컴퓨트용으로 각색했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.

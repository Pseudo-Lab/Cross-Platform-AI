# Eva로 연속 디스패치 작성하기

[13장](../13-multiple-dispatches/README.md)에서 만든 연속 디스패치를 [Eva](../../external/eva/src/eva-runtime.h)로 구현한 비교 예제입니다. [main.cpp](main.cpp)에서 전체 코드를 확인할 수 있습니다. 셰이더는 13장의 `add.comp`를 그대로 빌드합니다.

```text
uploadA/B → inputA/B → T = A + B → C = T + B → readback → CPU 검증
```

중간 결과는 GPU에 그대로 두고 두 디스패치를 하나의 커맨드 버퍼에 기록합니다. 같은 커맨드 버퍼를 두 번 제출하되, 두 번째 실행에서는 입력값을 바꿉니다.

## Vulkan-Hpp와 Eva 코드 비교

| 13장에서 직접 작성하는 부분 | Eva 코드 |
|---|---|
| 인스턴스, 물리 장치, 논리 장치, 컴퓨트 큐 준비 | `Runtime::get().device({.enableComputeQueues = true})` |
| 버퍼 생성, 메모리 타입 선택, 할당과 바인딩 | `device.createBuffer(...)` |
| 셰이더 모듈, 디스크립터 셋 레이아웃, 푸시 상수 범위와 파이프라인 레이아웃 | `createComputePipeline(...)`에서 SPIR-V 리플렉션으로 구성 |
| 디스크립터 풀 생성과 셋 할당 | `createDescriptorPool(...)`, `pool(pipeline.descSetLayout())` |
| 바인딩 0, 1, 2에 버퍼 연결 | `firstSet.write({inputA, inputB, intermediate})` |
| 커맨드 풀과 커맨드 버퍼 할당 | `device.newCommandBuffer(...)` |
| 단계와 접근 마스크를 지정한 버퍼 배리어 | `앞 접근 / 버퍼 / 뒤 접근` |
| 큐 제출, 펜스 대기와 재설정 | `queue << commandBuffer << fence`, `fence.wait(true)` |

두 디스크립터 셋은 같은 레이아웃을 사용합니다. 첫 번째 디스패치에는 `firstSet`을, 두 번째에는 `secondSet`을 set 0에 바인딩합니다. 파이프라인과 푸시 상수는 그대로 사용합니다.

```cpp
.bindPipeline(pipeline)
.setPushConstants(0, sizeof(elementCount), &elementCount)
.bindDescSets({firstSet})
.dispatch(groupCount)
.barrier(SYNC_SCOPE::COMPUTE_WRITE / intermediate / SYNC_SCOPE::COMPUTE_READ)
.bindDescSets({secondSet})
.dispatch(groupCount)
```

두 번째 디스패치는 첫 번째 디스패치가 `intermediate`에 쓴 값을 읽습니다. 사이에 넣은 배리어는 쓰기가 끝난 뒤 다음 셰이더가 그 결과를 읽을 수 있도록 동기화합니다. [Khronos 동기화 예제](https://docs.vulkan.org/guide/latest/synchronization_examples.html#compute-to-compute-dependencies)도 같은 상황을 다룹니다.

Eva가 필요한 배리어를 자동으로 넣어주지는 않으므로 개발자가 코드에 직접 지정해야 합니다. 이 예제에서는 입력 복사 후 셰이더가 읽기 전, 두 디스패치 사이, 최종 결과를 복사하기 전, 리드백 버퍼를 CPU가 읽기 전에 배리어를 넣습니다.

`begin()`을 기본 설정으로 호출하면 같은 커맨드 버퍼를 다시 제출할 수 있습니다. `fence.wait(true)`는 GPU 작업이 끝날 때까지 기다린 뒤 펜스를 재설정합니다. 대기가 성공하면 다음 실행을 위해 입력과 리드백 버퍼를 덮어써도 됩니다. 호스트 버퍼는 `HOST_COHERENT` 메모리를 사용하므로 별도의 flush와 invalidate는 하지 않습니다.

`vk::raii`와 달리 Eva에서는 지역 변수 `pool`이나 `ComputeApplication` 객체가 사라져도 GPU 리소스가 바로 해제되지 않습니다. 이 예제의 디바이스와 리소스는 프로그램이 종료될 때 Eva의 `Runtime`이 정리합니다.

## 빌드와 실행

Vulkan 1.3 이상 SDK와 C++23 컴파일러가 필요합니다. 저장소 루트에서 다음 명령으로 Eva와 하위 의존성을 저장소에 지정된 버전으로 내려받고 예제 폴더로 이동합니다.

```sh
git submodule update --init --recursive
cd examples/14-eva-multiple-dispatches
```

### Windows

`$env:VULKAN_SDK`가 SDK 설치 폴더로 설정되어 있는지 확인합니다. 비어 있다면 아래처럼 실제 설치 경로를 지정합니다.

```powershell
$env:VULKAN_SDK = 'C:\VulkanSDK\<설치한 버전>'
```

```powershell
cmake -S . -B build
cmake --build build --config Debug --target eva_multiple_dispatches_example
.\build\Debug\main.exe
ctest --test-dir build -C Debug --output-on-failure
```

### Linux

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target eva_multiple_dispatches_example
./build/main
ctest --test-dir build --output-on-failure
```

CMake는 `build/validation/vk_layer_settings.txt`를 생성합니다. CTest는 `VK_LAYER_KHRONOS_validation`을 활성화하고 이 설정으로 동기화 검증과 셰이더 접근의 정적 검사를 켭니다. 검증 메시지는 터미널에 출력됩니다. 계산 결과가 CPU 정답과 다르거나 Vulkan 검증 오류가 나오면 테스트가 실패합니다. Vulkan SDK의 `VK_LAYER_KHRONOS_validation`이 설치되어 있어야 합니다.

`elementCount = 8`인 기본 설정으로 실행하면 다음 결과가 나옵니다.

```text
Command buffer recorded: 1 workgroups per dispatch, 2 dispatches.
Round 1: verified 8 elements; first=20, last=41
Round 2: verified 8 elements; first=23, last=44
```

Eva가 출력하는 장치 정보는 GPU에 따라 달라집니다.

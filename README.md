# Cross-Platform-AI

Vulkan 기반 GPU 추론의 동작 원리를 코드로 살펴보는 문서·예제 프로젝트입니다. GPU에 데이터를 전달하고 컴퓨트 셰이더를 실행하는 기초부터, llama.cpp의 Vulkan 백엔드 분석, vAi를 이용한 커널 구현·최적화와 모델 실행까지 다룹니다.

서로 다른 GPU에서 같은 추론 코드를 실행하며 재사용할 수 있는 부분과 장치별로 조정해야 하는 부분을 확인합니다. 직접 연산을 수정하고 계산 결과와 실행 시간의 변화를 재현할 수 있는 자료를 만드는 것이 목표입니다.

> 문서와 실행 예제는 준비 중이며, 순차적으로 공개할 예정입니다.

## 다루는 내용

### Vulkan 컴퓨트 기초

Device와 Queue, Buffer와 Memory, descriptor를 이용한 리소스 연결부터 시작합니다. GLSL 셰이더를 SPIR-V로 컴파일하고 compute pipeline과 command buffer로 실행하는 최소 예제를 통해 데이터 전달, dispatch, 동기화, 결과 확인 과정을 설명합니다.

### llama.cpp Vulkan 백엔드 분석

llama.cpp의 실행 진입점에서 ggml을 거쳐 Vulkan 백엔드로 연산이 전달되는 경로를 추적합니다. 한 연산을 기준으로 셰이더 준비, 입력 형상에 따른 커널 선택, 파이프라인 생성·재사용, 리소스 연결, 명령 기록·제출을 살펴보고 prefill과 decode의 실행 흐름을 비교합니다.

### vAi 커널 구현과 최적화

그래프 DSL 기반 Vulkan 추론 엔진인 vAi의 Node·Tensor와 셰이더 실행 구조를 살펴보고 llama.cpp의 구현과 비교합니다. 커스텀 연산을 정의해 직접 작성한 셰이더를 연결하고, CPU 기준값으로 정확성을 검증합니다. 메모리 접근, 데이터 재사용, 워크그룹 구성을 바꾸며 최적화가 효과를 내는 조건과 한계를 측정합니다.

### 모델 구성과 실행

간단한 모델(예: GPT-2)의 가중치, 중간 텐서, 입출력을 연산 그래프로 연결합니다. 직접 작성한 연산을 모델에 적용하고 기본 커널과 개선한 커널의 계산 결과, 개별 커널 실행 시간, 모델 전체 실행 시간을 비교합니다.

## 공개 로드맵

| 단계 | 공개할 문서와 예제 |
|---|---|
| 1. Vulkan 기초 | 환경 구성·빌드 안내, 버퍼와 리소스 연결 설명, 컴퓨트 셰이더 실행 예제, 정확성 검사 |
| 2. llama.cpp 분석 | 실행 경로 그림, 핵심 코드 위치, 커널 선택·파이프라인·명령 제출까지 이어지는 백엔드 탐색 가이드 |
| 3. vAi 커널 구현 | 두 엔진의 구현 비교, 커스텀 연산 연결 예제, 기본 커널과 정확성 테스트 |
| 4. 최적화 실험 | 기본·개선 커널, 측정 코드와 원자료, 입력 조건별 효과와 한계 |
| 5. 모델 실행 | 모델 구성도, 가중치 준비·실행 안내, 입력부터 출력까지 동작하는 예제와 재현 결과 |

실험 기록에는 GPU, 드라이버, 코드 버전, 입력 조건, 실행 방법을 함께 남깁니다. 정확성은 여러 입력 크기와 경계 조건에서 확인하고, 성능은 동일한 GPU와 입력 조건에서 반복 측정합니다. 성능이 개선되지 않은 실험도 함께 기록합니다.

## Team

TBD

## Acknowledgement

이 프로젝트는 가짜연구소 Open Academy로 진행됩니다. 여러분의 참여와 기여가 '우연한 혁명(Serendipity Revolution)'을 가능하게 합니다. 모두에게 깊은 감사를 전합니다.

This project is developed as part of Pseudo-Lab's Open Research Initiative. Special thanks to our contributors and the open source community for their valuable insights and contributions.

### About Pseudo Lab

Pseudo-Lab is a non-profit organization focused on advancing machine learning and AI technologies. Our core values of Sharing, Motivation, and Collaborative Joy drive us to create impactful open-source projects. With over 5k+ researchers, we are committed to advancing machine learning and AI technologies.

## License

[MIT](LICENSE)

# 진행 순서

**현재 상태 (2026-09-03): Step 2 완료, 머지됨. Step 3 시작 전.**

- Step 0 — 개념 학습(RAII, 참조/포인터, vector, thread/mutex, 헤더분리)은 대화로 진행, 별도 산출물 없음
- Step 1 — `src/net/Socket.h`·`Socket.cpp`(RAII 소켓 래퍼, 복사금지·이동허용) +
  `src/main.cpp`(연결당 스레드 에코 서버) 완료. `nc`로 에코 동작 확인.
  `docs/decisions/01-io-model.md` 작성 완료. PR #1로 `main`에 머지됨
- Step 2 — `src/net/Packet.h`·`Packet.cpp`(PacketHeader 필드별 직렬화/역직렬화) +
  `main.cpp`의 부분 수신 루프(`drainPackets`)로 패킷 프레이밍 완성. doctest 왕복
  테스트 4종 통과, 실서버로 분할/합쳐진 패킷 echo 수동 검증 완료. PR #2로 `main`에 머지됨
- Step 3 — 아직 시작 안 함 (tick 루프 + naive 브로드캐스트, **완료 시 `git tag v0.1-naive` 필수**)

각 Step은 **커밋 가능한 상태**로 끝난다. 중간에 멈춰도 제출 가능하도록 설계했다.

---

## Step 0 — C++ 최소 문법

에이전트가 개념 설명, 사용자가 손으로 쳐보는 단계. 프로젝트 코드는 아직 안 짠다.

- 클래스, 생성자·소멸자, **RAII**
- 참조(`&`)와 포인터의 차이, `const`
- `std::vector`, `std::string` — C의 `malloc`/`realloc`과 무엇이 다른가
- `std::thread`, `std::mutex`, `std::lock_guard`
- 헤더/소스 분리, `#pragma once`

**목표**: "이 코드가 언제 메모리를 해제하는가"에 답할 수 있는 것.
스마트 포인터는 필요해질 때 배운다. 미리 배우면 안 쓸 곳에 쓴다.

## Step 1 — RAII 소켓 래퍼 + 에코 서버

- 소멸자에서 `close`하는 `Socket` 클래스
- `listen` / `accept` / 연결당 스레드
- 클라이언트가 보낸 문자열을 그대로 돌려주기

**여기가 C++ 학습 속도를 재는 지점이다.** 막힘없이 넘어가면 이후 일정을 당기고,
절반쯤에서 멈추면 계획대로 간다.

## Step 2 — 패킷 프레이밍 + 직렬화

- 공유 헤더에 패킷 정의
- `length`로 경계 자르기, **부분 수신 루프**
- 필드별 직렬화 / 역직렬화
- **왕복 테스트** (doctest 헤더 하나 vendoring)

## Step 3 — tick 루프 + naive 브로드캐스트

- 고정 틱 루프 (`std::chrono::steady_clock`)
- 명령 큐(수신→tick), 송신 큐(tick→송신)
- 전원에게 브로드캐스트 + narrow phase
- 지표 수집

**끝나면 `git tag v0.1-naive`.** 이걸 빠뜨리면 before/after를 증명할 수 없다.

## Step 4 — grid AOI

- 셀 배열, 좌표→인덱스, cell transition 시 remove/insert
- 3×3 순회로 후보 수집
- `--mode`로 naive와 전환

## Step 5 — 봇 + 측정

- 봇 클라이언트, `--pattern` 4종, `--seed`, 램프업
- `bench.sh`로 두 모드 연속 실행
- `docs/BENCH.md`에 조건과 함께 기록

## Step 6 — 아이템 거래

SPEC의 3단계 (naive → 데드락 → 순서 고정). TSan 전용 빌드로 검증.

## Step 7 — 문서

README, AI-LOG 정리, BENCH 마무리. 여유가 있으면 Colima로 리눅스 빌드 확인.

---

# 빌드 디렉토리 세 개

성능 측정과 샌디타이저를 섞으면 숫자가 무의미해진다.

```bash
cmake -B build       -DCMAKE_BUILD_TYPE=Release                   # 벤치마크
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug                     # 단위 테스트
cmake -B build-tsan  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON    # 동시성
```

**TSan은 5~15배 느리다.** 벤치는 반드시 Release, 샌디타이저 없이.

> 확인 완료: TSan은 arm64 macOS(Apple clang 21)에서 정상 동작한다. 리눅스 불필요.

---

# Git

- 커밋 이메일이 GitHub `dh610` 계정에 등록돼 있는지 **먼저 확인**
  (이전 프로젝트에서 `dh610`/`ddhhy` 두 identity로 갈려 기여도가 나뉜 적 있음)
- Conventional Commits (`feat:` / `fix:` / `perf:` / `docs:` / `test:`)
- 기능은 브랜치로 떼서 **자기 저장소에 PR을 올리고 머지**
- **naive 버전에 태그**

---

# 미결

- **8. entity cap을 넣을지** — Step 6~7에 여유가 있으면. 없으면 next step에만 기록

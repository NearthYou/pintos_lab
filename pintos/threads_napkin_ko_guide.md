# Pintos Threads 도식화 읽기 가이드

이 가이드는 `threads_napkin_ko.svg`를 보면서 Pintos threads 구현 범위를 이해하기 위한 보조 설명이다.
도식은 정답지가 아니라, 구현할 빈자리를 찾기 위한 지도처럼 보면 된다.

## 전체 읽는 법

도식은 왼쪽에서 오른쪽으로 읽는다.

```text
테스트 -> 기대 동작 -> 코드 위치 -> 채워야 할 빈칸
```

- `테스트`: 어떤 테스트 묶음이 이 기능을 요구하는지 보여준다.
- `기대 동작`: 테스트가 실제로 확인하려는 운영체제 동작이다.
- `코드 위치`: 주로 수정하거나 읽어야 하는 파일과 함수다.
- `채워야 할 빈칸`: 직접 설계해야 할 필드, helper 함수, 조건식이다.

## 색상 의미

| 색 | 주제 | 먼저 볼 테스트 |
| --- | --- | --- |
| 파랑 | Alarm Clock | `alarm-*` |
| 초록 | Priority Scheduling | `priority-preempt`, `priority-change`, `priority-fifo`, `priority-sema`, `priority-condvar` |
| 노랑 | Priority Donation | `priority-donate-*` |
| 보라 | MLFQS | `mlfqs-*` |
| 회색 | 구현 순서와 검증 지점 | 전체 진행 체크 |

## 화살표 의미

- `요구`: 테스트가 어떤 동작을 요구하는지 연결한다.
- `수정`: 그 동작을 만들기 위해 어느 코드 위치를 봐야 하는지 연결한다.
- `채움`: 코드에서 어떤 빈칸을 채워야 하는지 연결한다.
- `검증`: 구현 뒤 어떤 테스트 그룹으로 확인할지 연결한다.

## 빈칸 보는 법

`________`는 아직 이름을 정하지 않았거나 직접 설계해야 하는 부분이다.

예를 들어:

```text
int64_t ________, list_elem ________
```

이건 alarm clock을 위해 `struct thread`에 “언제 깨어날지”와 “sleep list에 들어갈 원소”가 필요하다는 뜻이다.
구체적인 이름은 직접 정하면 된다. 예: `wake_tick`, `sleep_elem` 같은 식.

## Alarm Clock 행

핵심 목표는 `timer_sleep()`이 CPU를 계속 양보하며 기다리는 방식이 아니라, 현재 스레드를 재워두고 timer interrupt에서 깨우는 방식으로 바뀌는 것이다.

봐야 할 곳:

- `pintos/devices/timer.c`
- `timer_sleep()`
- `timer_interrupt()`
- `pintos/include/threads/thread.h`

생각할 빈칸:

- 잠든 스레드들을 담을 전역 리스트 이름
- 각 스레드가 깨어날 tick 필드
- sleep list에 넣을 별도 `list_elem`
- timer interrupt에서 깨울 조건

주의할 점:

- `ticks <= 0`이면 바로 반환해야 한다.
- `timer_interrupt()`는 interrupt context에서 실행된다.
- 기존 `thread->elem`은 ready list나 semaphore waiters에서도 쓰이므로, sleep list용 원소를 따로 두는 편이 안전하다.

## Priority Scheduling 행

핵심 목표는 ready 상태나 waiter 상태의 스레드 중 우선순위가 가장 높은 스레드가 먼저 실행되게 하는 것이다.

봐야 할 곳:

- `pintos/threads/thread.c`
- `thread_unblock()`
- `thread_yield()`
- `next_thread_to_run()`
- `thread_set_priority()`
- `pintos/threads/synch.c`
- `sema_down()`, `sema_up()`, `cond_wait()`, `cond_signal()`

생각할 빈칸:

- priority 비교 함수
- ready list에 삽입하는 규칙
- 새로 깨어난 스레드가 현재 스레드보다 높을 때 yield할 조건
- semaphore/condition waiter를 깨우는 순서

주의할 점:

- 높은 priority가 먼저지만, 같은 priority끼리는 FIFO처럼 동작해야 한다.
- interrupt 안에서는 바로 `thread_yield()`를 부를 수 없고 `intr_yield_on_return()`을 고려해야 한다.
- `thread_set_priority()`로 현재 priority를 낮췄다면 더 높은 ready thread에게 CPU를 넘겨야 한다.

## Priority Donation 행

핵심 목표는 낮은 우선순위 스레드가 lock을 잡고 있고, 높은 우선순위 스레드가 그 lock을 기다릴 때 priority inversion을 줄이는 것이다.

봐야 할 곳:

- `pintos/threads/synch.c`
- `lock_acquire()`
- `lock_release()`
- `sema_down()`, `sema_up()`
- `pintos/include/threads/thread.h`
- `pintos/include/threads/synch.h`

생각할 빈칸:

- 원래 priority를 저장할 필드
- 현재 기다리는 lock을 가리키는 필드
- 나에게 donation한 스레드 목록
- lock release 때 어떤 donation을 제거할지 판단하는 기준
- donation을 lock chain을 따라 전파하는 helper

주의할 점:

- donation 받은 priority와 원래 priority를 구분해야 한다.
- lock을 release하면 해당 lock 때문에 받은 donation은 사라져야 한다.
- nested/chain donation 테스트는 donation이 여러 lock holder를 따라 전파되는지 확인한다.
- MLFQS 모드에서는 priority donation 테스트가 돌지 않는다.

## MLFQS 행

핵심 목표는 priority를 직접 고정하지 않고, `nice`, `recent_cpu`, `load_avg`로 주기적으로 계산하는 것이다.

봐야 할 곳:

- `pintos/threads/thread.c`
- `thread_tick()`
- `thread_set_nice()`
- `thread_get_nice()`
- `thread_get_recent_cpu()`
- `thread_get_load_avg()`
- `pintos/include/threads/thread.h`

생각할 빈칸:

- fixed-point 표현 방식
- `nice` 필드
- `recent_cpu` 필드
- 전역 `load_avg`
- 1초마다 갱신할 값
- 4 tick마다 갱신할 값

공식 식:

```text
priority = PRI_MAX - recent_cpu / 4 - nice * 2
recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
load_avg = (59 / 60) * load_avg + (1 / 60) * ready_threads
```

주의할 점:

- 정수만 쓰면 소수 계산이 깨지므로 fixed-point 계산이 필요하다.
- idle thread는 `recent_cpu` 계산에서 제외한다.
- priority 범위는 `PRI_MIN`에서 `PRI_MAX` 사이로 clamp해야 한다.

## 추천 진행 순서

1. `alarm-*` 행만 보고 alarm clock 의사코드를 먼저 쓴다.
2. `timer.c`와 `thread.h`에 필요한 빈칸 이름을 정한다.
3. alarm 테스트를 통과시킨다.
4. priority 행으로 넘어가 ready list와 waiter list 정렬을 설계한다.
5. donation 행으로 넘어가 base/effective priority를 분리한다.
6. 마지막에 MLFQS를 따로 구현한다.

## 도식 파일

- 한국어 SVG: `pintos/threads_napkin_ko.svg`
- 한국어 수정용 JSON: `pintos/threads_napkin_ko_spec.json`
- 영어 SVG: `pintos/threads_napkin.svg`
- 영어 수정용 JSON: `pintos/threads_napkin_spec.json`

# Pintos Project 1 Threads 테스트케이스 분석 정리

이 문서는 `pintos/tests/threads` 아래의 테스트들을 **함수 구현 관점**에서 정리한 메모다.

목표는 딱 하나다.

- 테스트를 읽고
- "이 테스트가 어떤 상황을 만들고"
- "무슨 규칙을 강제하며"
- "그래서 어느 함수를 구현해야 하는지"

를 빠르게 연결하는 것.

---

## 1. 먼저 보는 큰 그림

Project 1 Threads 테스트는 크게 3묶음이다.

1. Alarm Clock
2. Priority Scheduling + Priority Donation
3. MLFQS

핵심 질문은 항상 같다.

- 지금 이 thread는 왜 CPU를 못 쓰는가?
- 지금 READY / BLOCKED 중 어디에 있어야 하는가?
- 누가 다시 READY로 만드는가?
- 그 READY thread들 중 누가 먼저 RUNNING이 되어야 하는가?

---

## 2. 구현 파일 매핑

테스트를 보다 보면 결국 수정 대상은 거의 아래 파일로 모인다.

### `pintos/devices/timer.c`
- `timer_sleep()`
- `timer_interrupt()`

### `pintos/threads/thread.c`
- `thread_unblock()`
- `thread_yield()`
- `next_thread_to_run()`
- `thread_set_priority()`
- MLFQS 관련 갱신 함수들

### `pintos/threads/synch.c`
- `sema_down()`
- `sema_up()`
- `lock_acquire()`
- `lock_release()`
- `cond_wait()`
- `cond_signal()`

### `pintos/include/threads/thread.h`
- `struct thread`에 필요한 필드 추가
- 예: `wakeup_tick`, `base_priority`, `waiting_lock`, donation 관련 리스트, `nice`, `recent_cpu`

---

## 3. 테스트 읽는 기본 법칙

테스트 코드는 한 줄 한 줄 C 문법을 해석하는 게 아니라 아래 4개를 읽으면 된다.

1. **무슨 상황을 만든다**
2. **기대 출력/기대 순서가 무엇이다**
3. **이 순서를 만들려면 어떤 상태 전이가 맞아야 한다**
4. **결국 어느 함수를 수정해야 한다**

테스트마다 아래 양식으로 메모하면 구현이 빨라진다.

```text
테스트 이름:
상황:
기대:
핵심 상태 변화:
관련 list:
관련 함수:
실패 시 먼저 의심할 곳:
```

---

## 4. Alarm 계열

Alarm 테스트는 거의 전부 이 요구사항으로 귀결된다.

- `timer_sleep()`은 busy waiting 하면 안 된다.
- sleeping thread는 `BLOCKED` 상태여야 한다.
- sleeping thread는 ready queue에 있으면 안 된다.
- 시간이 되면 timer interrupt가 깨워서 `READY`로 돌려야 한다.

### 공통 구현 포인트

- `timer_sleep(ticks)`에서
  - `ticks <= 0` 처리
  - wake-up 시각 계산
  - sleeper 저장
  - `thread_block()`
- `timer_interrupt()`에서
  - `ticks++`
  - 시간이 된 sleeper들을 `thread_unblock()`

### 관련 함수

- `timer_sleep()`
- `timer_interrupt()`
- `thread_block()`
- `thread_unblock()`

---

### 4.1 `alarm-single`, `alarm-multiple`
소스 파일: `pintos/tests/threads/alarm-wait.c`

#### 상황
- 여러 thread를 만든다.
- 각 thread는 서로 다른 길이로 잔다.
- 깨어날 때 자기 id를 기록한다.

#### 검증 포인트
- 짧게 자는 thread가 먼저 깨어나야 한다.
- 각 thread가 정확한 횟수만큼 깨어나야 한다.
- 너무 빨리 깨우거나, 너무 늦게 깨우면 순서가 깨진다.

#### 구현으로 연결되는 의미
- `timer_sleep()`이 READY/RUNNING을 반복하면 안 된다.
- 절전 중에는 CPU를 먹으면 안 된다.
- wake-up 시각 비교가 정확해야 한다.

#### 실패 시 먼저 볼 곳
- `timer_sleep()`
- `timer_interrupt()`
- sleeper 저장 방식

---

### 4.2 `alarm-simultaneous`
소스 파일: `pintos/tests/threads/alarm-simultaneous.c`

#### 상황
- 여러 thread를 같은 시각에 깨운다.

#### 검증 포인트
- 같은 tick에 깨어나야 하는 애들을 놓치지 않고 깨우는지
- 같은 tick에 여러 명이 READY가 될 수 있는지

#### 구현으로 연결되는 의미
- `timer_interrupt()`에서 "시간 된 첫 번째 하나만" 깨우면 안 된다.
- 현재 tick 기준으로 깨어나야 할 sleeper를 모두 처리해야 한다.

#### 실패 시 먼저 볼 곳
- `timer_interrupt()`의 wake-up loop
- sleep list 정렬/순회 조건

---

### 4.3 `alarm-priority`
소스 파일: `pintos/tests/threads/alarm-priority.c`

#### 상황
- 여러 thread가 같은 시각에 깨어난다.
- 하지만 priority는 다르다.

#### 검증 포인트
- wake-up은 동시에 맞아야 한다.
- 그다음 실행 순서는 높은 priority부터여야 한다.

#### 구현으로 연결되는 의미
- Alarm만 맞아도 안 되고 priority scheduler도 맞아야 한다.
- `thread_unblock()` 후 ready queue 정렬/선택이 중요하다.

#### 실패 시 먼저 볼 곳
- `timer_interrupt()`
- `thread_unblock()`
- `next_thread_to_run()`

---

### 4.4 `alarm-zero`
소스 파일: `pintos/tests/threads/alarm-zero.c`

#### 상황
- `timer_sleep(0)` 호출

#### 검증 포인트
- 즉시 반환해야 한다.

#### 실패 시 먼저 볼 곳
- `timer_sleep()` 초반 예외 처리

---

### 4.5 `alarm-negative`
소스 파일: `pintos/tests/threads/alarm-negative.c`

#### 상황
- `timer_sleep(음수)` 호출

#### 검증 포인트
- block되거나 망가지지 말고 그냥 지나가야 한다.

#### 실패 시 먼저 볼 곳
- `timer_sleep()`의 `ticks <= 0` 처리

---

## 5. Priority Scheduling 계열

이 묶음은 핵심적으로 이런 규칙을 강제한다.

- READY 중 가장 높은 priority가 먼저 RUNNING
- 더 높은 priority thread가 READY 되면 현재 thread는 양보
- 같은 priority 내부 순서도 지나치게 깨지면 안 됨
- semaphore / condition variable waiters도 priority를 반영해야 함

### 관련 함수

- `thread_unblock()`
- `next_thread_to_run()`
- `thread_yield()`
- `thread_set_priority()`
- `sema_down()`
- `sema_up()`
- `cond_wait()`
- `cond_signal()`

---

### 5.1 `priority-change`
소스 파일: `pintos/tests/threads/priority-change.c`

#### 상황
- 실행 중인 thread가 자기 priority를 낮춘다.

#### 검증 포인트
- 더 이상 최고 priority가 아니면 즉시 `yield()` 해야 한다.

#### 구현으로 연결되는 의미
- `thread_set_priority()`는 값만 바꾸는 함수가 아니다.
- 바꾼 뒤 스케줄링 재판단이 필요하다.

#### 실패 시 먼저 볼 곳
- `thread_set_priority()`
- `thread_yield()`

---

### 5.2 `priority-preempt`
소스 파일: `pintos/tests/threads/priority-preempt.c`

#### 상황
- 낮은 priority thread가 실행 중이다.
- 더 높은 priority thread가 새로 READY 된다.

#### 검증 포인트
- 높은 priority thread가 즉시 선점해야 한다.

#### 구현으로 연결되는 의미
- `thread_create()` 후 또는 `thread_unblock()` 후
  current보다 높은 priority가 READY 되면 양보해야 한다.

#### 실패 시 먼저 볼 곳
- `thread_unblock()`
- `next_thread_to_run()`
- 필요시 `thread_create()` 말미 선점 처리

---

### 5.3 `priority-fifo`
소스 파일: `pintos/tests/threads/priority-fifo.c`

#### 상황
- 같은 priority의 thread들이 lock을 잡고 기록한 뒤 `yield()`를 반복한다.

#### 검증 포인트
- 같은 priority끼리는 순서가 일관적이어야 한다.

#### 구현으로 연결되는 의미
- priority만 맞추고 같은 priority 내부 순서를 망가뜨리면 실패할 수 있다.
- ready queue 삽입 정책이 중요하다.

#### 실패 시 먼저 볼 곳
- ready queue 삽입 위치
- `thread_unblock()`
- `next_thread_to_run()`

---

### 5.4 `priority-sema`
소스 파일: `pintos/tests/threads/priority-sema.c`

#### 상황
- 여러 thread가 semaphore waiters에 걸려 있다.
- priority가 다르다.

#### 검증 포인트
- `sema_up()`가 가장 높은 priority waiter를 깨워야 한다.

#### 구현으로 연결되는 의미
- semaphore waiters를 FIFO로만 두면 안 된다.
- priority-aware waiter selection이 필요하다.

#### 실패 시 먼저 볼 곳
- `sema_down()`
- `sema_up()`
- waiters 리스트 정렬/최댓값 선택

---

### 5.5 `priority-condvar`
소스 파일: `pintos/tests/threads/priority-condvar.c`

#### 상황
- 여러 thread가 `cond_wait()` 중이다.
- 메인이 여러 번 `cond_signal()` 한다.

#### 검증 포인트
- `cond_signal()`이 가장 높은 priority waiter를 깨워야 한다.

#### 구현으로 연결되는 의미
- condvar waiters도 priority-aware여야 한다.
- 내부적으로 semaphore_elem 우선순위를 비교해야 할 가능성이 크다.

#### 실패 시 먼저 볼 곳
- `cond_wait()`
- `cond_signal()`
- condvar waiters 비교 함수

---

## 6. Priority Donation 계열

Donation 묶음은 전부 이 문제를 다룬다.

- 낮은 priority thread가 lock을 들고 있음
- 높은 priority thread가 그 lock을 기다림
- 그러면 높은 priority를 lock holder에게 빌려줘야 함

여기서 핵심은 보통 두 값의 분리다.

- `base_priority`
- `effective priority`

그리고 보통 아래 필드/구조가 필요하다.

- `waiting_lock`
- donation list
- lock별 donation 회수 정보

### 관련 함수

- `lock_acquire()`
- `lock_release()`
- `thread_set_priority()`
- priority refresh helper
- donation add/remove helper

---

### 6.1 `priority-donate-one`
소스 파일: `pintos/tests/threads/priority-donate-one.c`

#### 상황
- 메인이 lock을 보유
- 더 높은 priority 2개가 같은 lock을 기다림

#### 검증 포인트
- 메인이 donation을 받아 priority가 올라가야 한다.
- lock release 후 waiter는 높은 priority 순서로 lock을 가져야 한다.

#### 구현으로 연결되는 의미
- donation 기본 동작
- lock release 후 wake-up 순서

#### 실패 시 먼저 볼 곳
- `lock_acquire()`
- `lock_release()`
- `sema_up()` / lock 내부 semaphore

---

### 6.2 `priority-donate-multiple`
소스 파일: `pintos/tests/threads/priority-donate-multiple.c`

#### 상황
- 메인이 lock A, B를 모두 잡고 있다.
- 서로 다른 high-priority thread가 각각 A, B를 기다린다.

#### 검증 포인트
- 가장 높은 donation이 current effective priority를 결정해야 한다.
- lock 하나를 풀면 그 lock 관련 donation만 사라져야 한다.

#### 구현으로 연결되는 의미
- donation을 하나만 저장하면 안 된다.
- lock별로 donation 회수가 가능해야 한다.

#### 실패 시 먼저 볼 곳
- donation 저장 구조
- `lock_release()` 회수 로직
- priority 재계산 함수

---

### 6.3 `priority-donate-multiple2`
소스 파일: `pintos/tests/threads/priority-donate-multiple2.c`

#### 상황
- multiple과 비슷하지만 lock release 순서가 다르다.
- unrelated thread도 끼어 있다.

#### 검증 포인트
- release 순서가 바뀌어도 priority 재계산이 맞아야 한다.
- lock과 무관한 thread가 순서를 깨면 안 된다.

#### 구현으로 연결되는 의미
- "모든 donation 삭제" 같은 거친 구현이 실패한다.
- 정확히 "이 lock 때문에 들어온 donation만" 제거해야 한다.

#### 실패 시 먼저 볼 곳
- `lock_release()`
- donation remove helper

---

### 6.4 `priority-donate-lower`
소스 파일: `pintos/tests/threads/priority-donate-lower.c`

#### 상황
- 메인이 donation을 받은 상태에서 자기 base priority를 낮춘다.

#### 검증 포인트
- donation이 살아있는 동안 effective priority는 유지돼야 한다.
- lock을 풀고 donation이 사라진 뒤에야 낮춘 base priority가 반영돼야 한다.

#### 구현으로 연결되는 의미
- `thread_set_priority()`는 donation 중일 때 base만 바꾸고
  effective는 별도 계산해야 한다.

#### 실패 시 먼저 볼 곳
- `thread_set_priority()`
- priority refresh helper

---

### 6.5 `priority-donate-nest`
소스 파일: `pintos/tests/threads/priority-donate-nest.c`

#### 상황
- L이 lock A를 보유
- M이 lock B를 잡고 A를 기다림
- H가 B를 기다림

#### 검증 포인트
- H의 priority가 M을 거쳐 L까지 전달돼야 한다.

#### 구현으로 연결되는 의미
- donation은 한 단계가 아니라 체인으로 전파되어야 한다.
- `waiting_lock` 기반 추적이 필요하다.

#### 실패 시 먼저 볼 곳
- `lock_acquire()` 내부 donation 전파
- nested donation helper

---

### 6.6 `priority-donate-chain`
소스 파일: `pintos/tests/threads/priority-donate-chain.c`

#### 상황
- donation chain을 길게 7단계 이상 만든다.
- donor 사이사이에 interloper도 섞는다.

#### 검증 포인트
- 긴 체인 donation이 끝까지 전달되는지
- donor보다 낮은 interloper가 끼어들지 못하는지

#### 구현으로 연결되는 의미
- nested donation 일반화가 필요하다.
- donation depth가 깊어져도 올바르게 동작해야 한다.

#### 실패 시 먼저 볼 곳
- donation 반복/재귀 전파 로직
- priority refresh 로직

---

### 6.7 `priority-donate-sema`
소스 파일: `pintos/tests/threads/priority-donate-sema.c`

#### 상황
- low thread가 lock을 잡고 semaphore에서 block
- medium도 semaphore 대기
- high가 low의 lock을 기다리며 donation

#### 검증 포인트
- donation + semaphore wakeup + lock release가 섞인 복합 상황에서도 순서가 맞아야 한다.

#### 구현으로 연결되는 의미
- donation은 lock wait 때문에 생기고
- 실제 실행 재개는 sema wakeup 때문에 생길 수 있다.
- 복합 상태 전이가 맞아야 한다.

#### 실패 시 먼저 볼 곳
- `lock_acquire()`
- `lock_release()`
- `sema_up()`
- priority recompute

---

## 7. MLFQS 계열

MLFQS 모드에서는 의미가 완전히 달라진다.

- manual priority scheduling 중심이 아님
- donation도 쓰지 않음
- priority를 공식으로 계산함

핵심 업데이트 리듬:

- 매 tick: 현재 RUNNING thread의 `recent_cpu` 증가
- 매 1초: `load_avg`와 모든 thread의 `recent_cpu` 갱신
- 매 4 tick: 모든 thread priority 재계산

### 관련 함수

- `thread_tick()`
- `thread_set_nice()`
- `thread_get_nice()`
- `thread_get_recent_cpu()`
- `thread_get_load_avg()`
- `update_load_avg()`
- `update_recent_cpu_all()`
- `update_priority_all()`

---

### 7.1 `mlfqs-load-1`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-load-1.c`

#### 상황
- 바쁜 thread 하나만 존재

#### 검증 포인트
- `load_avg`가 적절한 속도로 0.5 근처까지 올라가야 한다.
- idle 시간이 생기면 다시 떨어져야 한다.

#### 구현으로 연결되는 의미
- `load_avg` 공식이 맞아야 한다.
- 1초 주기 갱신이 정확해야 한다.

#### 실패 시 먼저 볼 곳
- `thread_get_load_avg()`
- 1초 단위 `load_avg` update

---

### 7.2 `mlfqs-load-60`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-load-60.c`

#### 상황
- 60개 thread가 한동안 runnable

#### 검증 포인트
- runnable thread 수가 많을 때 `load_avg`가 제대로 증가/감소하는지

#### 구현으로 연결되는 의미
- ready thread 수 계산
- idle thread 제외

#### 실패 시 먼저 볼 곳
- ready_threads 계산
- `load_avg` update

---

### 7.3 `mlfqs-load-avg`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-load-avg.c`

#### 상황
- thread들이 서로 다른 시간에 runnable이 된다.

#### 검증 포인트
- 더 섬세한 load average 패턴이 맞는지
- timer interrupt가 너무 무거워서 메인 thread가 제때 sleep 못 하면 실패할 수 있다.

#### 구현으로 연결되는 의미
- interrupt handler에 불필요하게 비싼 일을 넣으면 안 된다.
- 갱신 주기별로 해야 할 일과 하지 말아야 할 일을 분리해야 한다.

#### 실패 시 먼저 볼 곳
- `timer_interrupt()` 안 무거운 루프
- 전체 thread 순회 타이밍

---

### 7.4 `mlfqs-recent-1`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-recent-1.c`

#### 상황
- ready thread 하나에서 `recent_cpu`와 `load_avg`를 길게 관측

#### 검증 포인트
- `recent_cpu` 공식이 정확한지
- 1초 경계에서 딱 맞게 갱신되는지

#### 구현으로 연결되는 의미
- `timer_ticks() % TIMER_FREQ == 0` 경계가 중요하다.
- 갱신 타이밍이 한 tick 어긋나면 오차가 누적된다.

#### 실패 시 먼저 볼 곳
- `recent_cpu` update
- 1초 경계 처리

---

### 7.5 `mlfqs-fair-2`, `mlfqs-fair-20`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-fair.c`

#### 상황
- 같은 nice 값의 thread들을 여러 개 돌린다.

#### 검증 포인트
- CPU가 대체로 공평하게 분배돼야 한다.

#### 구현으로 연결되는 의미
- priority 재계산과 ready scheduling이 공정해야 한다.

#### 실패 시 먼저 볼 곳
- priority 계산식
- ready queue 정렬/선택

---

### 7.6 `mlfqs-nice-2`, `mlfqs-nice-10`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-fair.c`

#### 상황
- 서로 다른 nice 값을 가진 thread들이 같이 실행된다.

#### 검증 포인트
- nice 값에 따라 CPU 분배 비율이 달라져야 한다.

#### 구현으로 연결되는 의미
- `thread_set_nice()`
- priority 계산식
- 재계산 타이밍이 중요하다.

#### 실패 시 먼저 볼 곳
- `thread_set_nice()`
- priority update

---

### 7.7 `mlfqs-block`
소스 파일: `pintos/tests/threads/mlfqs/mlfqs-block.c`

#### 상황
- block thread가 한동안 CPU를 쓰다가 lock wait로 BLOCKED 된다.

#### 검증 포인트
- blocked thread도 `recent_cpu`와 priority가 올바르게 갱신돼야 한다.
- unblock 직후 바로 스케줄될 수 있어야 한다.

#### 구현으로 연결되는 의미
- MLFQS 갱신은 RUNNING thread만 보는 게 아니다.
- 전체 thread에 적용되는 갱신이 있다.

#### 실패 시 먼저 볼 곳
- blocked thread 포함한 `recent_cpu` update
- priority recompute for all threads

---

## 8. 테스트 이름에서 바로 읽어낼 수 있는 규칙

### Alarm
- `alarm-*`:
  - sleep / wakeup / blocked / timer interrupt

### Priority
- `priority-change`:
  - priority 낮출 때 yield 되는가
- `priority-preempt`:
  - 높은 priority가 나타나면 즉시 선점되는가
- `priority-fifo`:
  - 같은 priority끼리 순서가 안정적인가
- `priority-sema`, `priority-condvar`:
  - waiters에서도 priority가 반영되는가

### Donation
- `priority-donate-one`:
  - 기본 donation
- `priority-donate-multiple`:
  - 여러 donation
- `priority-donate-lower`:
  - donation 중 base priority 변경
- `priority-donate-nest`, `chain`:
  - 연쇄 donation
- `priority-donate-sema`:
  - donation + semaphore 섞인 상황

### MLFQS
- `load-*`:
  - load_avg
- `recent-*`:
  - recent_cpu
- `fair-*`:
  - 공평한 CPU 분배
- `nice-*`:
  - nice가 priority에 미치는 영향
- `block`:
  - blocked thread 갱신

---

## 9. 구현 순서 추천

이 순서가 보통 제일 덜 꼬인다.

1. Alarm
   - `alarm-single`
   - `alarm-multiple`
   - `alarm-simultaneous`
   - `alarm-priority`
2. Priority basic
   - `priority-change`
   - `priority-preempt`
   - `priority-fifo`
3. Sync priority
   - `priority-sema`
   - `priority-condvar`
4. Donation basic
   - `priority-donate-one`
   - `priority-donate-multiple`
   - `priority-donate-multiple2`
   - `priority-donate-lower`
5. Donation advanced
   - `priority-donate-nest`
   - `priority-donate-chain`
   - `priority-donate-sema`
6. MLFQS
   - `mlfqs-load-1`
   - `mlfqs-recent-1`
   - `mlfqs-fair-*`
   - `mlfqs-load-*`
   - `mlfqs-block`

---

## 10. 의사코드 쓸 때 바로 필요한 함수 목록

### Alarm
- `timer_sleep()`
- `timer_interrupt()`

### Priority
- `thread_unblock()`
- `next_thread_to_run()`
- `thread_set_priority()`
- `thread_yield()`

### Waiters
- `sema_down()`
- `sema_up()`
- `cond_wait()`
- `cond_signal()`

### Donation
- `lock_acquire()`
- `lock_release()`
- `refresh_priority()`
- donation add/remove helper

### MLFQS
- `thread_tick()` 안 갱신 흐름
- `update_load_avg()`
- `update_recent_cpu_all()`
- `update_priority_all()`
- `thread_set_nice()`

---

## 11. 마지막 체크포인트

구현 중 계속 확인할 것:

- sleep 중인 thread는 READY가 아니라 BLOCKED인가?
- 시간이 되기 전에는 절대 wakeup하지 않는가?
- READY 중 가장 높은 priority가 선택되는가?
- semaphore / condvar waiters도 highest priority를 먼저 깨우는가?
- donation 중에는 base priority와 effective priority를 분리했는가?
- lock release 시 관련 donation만 제거하는가?
- MLFQS에서는 donation/manual priority와 분리했는가?
- 매 tick / 매 4 tick / 매 1초 갱신 타이밍이 정확한가?

---

## 12. 한 줄 요약

이 테스트 묶음 전체는 결국 아래를 구현하라고 요구한다.

- Alarm: `sleep -> BLOCKED -> timer interrupt -> READY`
- Priority: `highest priority READY thread -> RUNNING`
- Waiters: `waiters에서도 highest priority 먼저`
- Donation: `lock holder가 donor의 priority를 임시로 받기`
- MLFQS: `priority를 공식과 tick 리듬으로 자동 계산`


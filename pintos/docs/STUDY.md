# Pintos Project 1: Threads 학습 노트

> 커널 수준 스레드의 생성·스케줄링·동기화를 직접 구현하면서 OS의 핵심 메커니즘을 체득하는 과정.
> 개념 → Pintos 코드 → 구현 포인트 순으로 정리.

---

## 목차

1. [토대: 스레드, 컨텍스트 스위칭, 인터럽트](#1-토대)
2. [동기화: race condition과 프리미티브](#2-동기화)
3. [스케줄링: Priority와 Donation](#3-스케줄링-priority와-donation)
4. [MLFQS와 Fixed-Point 연산](#4-mlfqs와-fixed-point-연산)
5. [통합 정리와 구현 순서](#5-통합-정리)

---

# 1. 토대

## 1.1 스레드란 무엇인가

**프로세스 vs 스레드**
- 프로세스: 자원(메모리 공간, 파일 디스크립터 등)의 단위
- 스레드: **실행(execution)의 단위**. CPU가 실제로 스케줄링하는 대상

한 프로세스 안에 여러 스레드가 존재할 수 있고, 이들은 코드/데이터/힙은 공유하지만 **스택과 레지스터 상태(컨텍스트)는 각자 가짐**.

**커널 스레드 vs 유저 스레드**
- Pintos의 스레드는 **커널 스레드**: 커널 주소 공간에서 실행되며 스케줄링도 커널이 직접 함
- Project 1에서는 유저 프로그램이 없고, 모든 스레드가 커널 모드에서만 동작

## 1.2 스레드 상태 (State)

Pintos 스레드는 4가지 상태를 오감.

```
        thread_create()
              │
              ▼
        ┌──────────┐    schedule()    ┌──────────┐
        │  READY   │ ───────────────▶ │ RUNNING  │
        └──────────┘ ◀─────────────── └──────────┘
              ▲       thread_yield()       │
              │                            │
   thread_unblock()                  thread_block()
              │                            │
              │                            ▼
              │                      ┌──────────┐
              └────────────────────  │ BLOCKED  │
                                     └──────────┘
                                            │
                                       thread_exit()
                                            ▼
                                       ┌─────────┐
                                       │ DYING   │
                                       └─────────┘
```

| 상태 | 의미 | 어디에 있나? |
|------|------|--------------|
| `THREAD_RUNNING` | 현재 CPU에서 실행 중 | CPU |
| `THREAD_READY` | 실행 가능, CPU 대기 중 | `ready_list` |
| `THREAD_BLOCKED` | 어떤 사건을 기다리는 중 (I/O, lock, sleep 등) | 각 동기화 객체의 대기 리스트 |
| `THREAD_DYING` | 종료되어 곧 메모리 회수될 예정 | - |

> **핵심 통찰**: 모든 동기화 프리미티브는 결국 "스레드를 BLOCKED로 보내고, 적절한 시점에 READY로 깨우는" 동작입니다. 이 한 문장이 이해되면 semaphore/lock/CV가 전부 같은 패턴의 변형으로 보입니다.

## 1.3 컨텍스트 스위칭

**컨텍스트(context)**: CPU 레지스터들의 현재 값 (PC, SP, 범용 레지스터 등). 스레드의 "현재 상태"를 결정하는 모든 정보.

**컨텍스트 스위칭 과정**
1. 현재 스레드의 레지스터 값들을 그 스레드의 스택에 저장
2. 다음 스레드의 스택에서 레지스터 값들을 복원
3. PC가 복원되는 순간 → 다음 스레드의 코드가 실행됨

Pintos에서는 `threads/thread.c`의 `schedule()`과 `threads/threads.S`의 `thread_launch()` (또는 구버전 `switch_threads`)에서 일어납니다.

```c
// thread.c (단순화)
static void schedule(void) {
    struct thread *curr = running_thread();
    struct thread *next = next_thread_to_run();  // ready_list에서 꺼냄

    if (curr != next) {
        thread_launch(next);  // 어셈블리로 컨텍스트 스위치
    }
}
```

**왜 어셈블리로 작성하나?**
레지스터를 직접 다뤄야 하기 때문. C 컴파일러는 함수 호출 규약에 따라 레지스터를 임의로 사용하므로, 정밀한 제어가 불가능합니다.

## 1.4 인터럽트

**인터럽트의 종류**
- **External interrupt**: CPU 외부에서 발생 (타이머, 키보드, 디스크 I/O 등)
- **Internal interrupt** (예외, trap): CPU 내부에서 발생 (page fault, divide by zero, 시스템 콜 등)

**왜 OS 학습에 중요한가?**
- 스케줄링은 **타이머 인터럽트**가 주기적으로 발생해야 동작 (preemptive scheduling)
- 동기화는 **인터럽트 비활성화**로 critical section을 보호하는 경우가 많음

**Pintos의 인터럽트 제어**
```c
// 인터럽트 비활성화하고 이전 상태 반환
enum intr_level old_level = intr_disable();

// ... critical section ...

intr_set_level(old_level);  // 원래 상태로 복원
```

**`intr_context()` 의 의미**
현재 코드가 인터럽트 핸들러 내부에서 실행 중인지 확인.
- 인터럽트 핸들러 안에서는 **sleep/block 하면 안 됨** (어떤 스레드를 깨워야 할지 모호)
- 그래서 `thread_yield()`는 인터럽트 컨텍스트에서 직접 호출되지 않고, `intr_yield_on_return()`으로 "인터럽트 끝나고 yield하라"고 표시만 함

```c
void timer_interrupt(struct intr_frame *args) {
    ticks++;
    thread_tick();  // 내부에서 필요시 intr_yield_on_return() 호출
}
```

## 1.5 Pintos 스레드 구조체

```c
struct thread {
    tid_t tid;                          // 스레드 ID
    enum thread_status status;          // RUNNING/READY/BLOCKED/DYING
    char name[16];
    int priority;                       // 우선순위 (0~63)

    struct list_elem elem;              // ready_list, sema의 waiters 등에 들어갈 때 사용

    /* Project 1 추가 필드들 (구현해야 함) */
    int64_t wakeup_tick;                // alarm clock용
    int init_priority;                  // donation 받기 전 원래 우선순위
    struct lock *wait_on_lock;          // 현재 기다리는 lock
    struct list donations;              // 나에게 donation 한 스레드들
    struct list_elem donation_elem;

    /* MLFQS용 */
    int nice;
    int recent_cpu;                     // fixed-point

    unsigned magic;                     // stack overflow 감지
};
```

**주의: `magic` 필드**
스택 overflow를 감지하는 sentinel. 스택은 `thread` 구조체 바로 위쪽에서 자라기 때문에, overflow가 나면 `magic` 값이 덮어써집니다. `is_thread()`가 이걸로 유효성 검사를 함.


---

# 2. 동기화

## 2.1 Race Condition: 왜 동기화가 필요한가

**Race condition**: 여러 스레드가 공유 자원에 동시 접근할 때, 실행 순서에 따라 결과가 달라지는 현상.

**고전 예시: counter++**
```c
int counter = 0;

// 스레드 A, B가 동시에 실행
counter++;
```

`counter++`는 한 줄처럼 보이지만 기계어로는 3단계입니다.
```
LOAD  R1, [counter]   ; 메모리 → 레지스터
ADD   R1, 1           ; 레지스터에서 +1
STORE [counter], R1   ; 레지스터 → 메모리
```

A가 LOAD까지만 하고 인터럽트로 B에 CPU를 뺏기면? B가 STORE까지 끝낸 뒤 A가 돌아와 자기 R1 값을 STORE → B의 결과가 사라짐.

**Critical Section**: 공유 자원에 접근하는 코드 영역. 한 번에 한 스레드만 들어가야 함.

## 2.2 동기화의 기본 도구: 인터럽트 비활성화

**가장 단순한 방법** (단일 CPU 환경에서):
```c
enum intr_level old = intr_disable();
counter++;  // 이 줄 동안 컨텍스트 스위칭 불가
intr_set_level(old);
```

**문제점**
- 인터럽트를 오래 끄면 시스템 응답성 저하
- 멀티 CPU에서는 다른 코어의 동시 접근을 막지 못함
- 그래서 짧은 critical section이나 다른 동기화 도구 자체를 구현할 때만 사용

## 2.3 Semaphore: 모든 동기화의 기초

**개념**: 정수 카운터 + 두 연산
- `P()` (down, wait, sema_down): 값이 0이면 대기, 아니면 값을 1 감소시키고 진행
- `V()` (up, signal, sema_up): 값을 1 증가시키고, 대기 중인 스레드 하나 깨움

```c
struct semaphore {
    unsigned value;              // 카운터
    struct list waiters;         // 대기 중인 스레드 리스트
};
```

**Pintos 구현 (synch.c)**
```c
void sema_down(struct semaphore *sema) {
    enum intr_level old_level = intr_disable();

    while (sema->value == 0) {
        // 1. 자신을 waiters 리스트에 넣고
        list_push_back(&sema->waiters, &thread_current()->elem);
        // 2. BLOCKED 상태로 전환 (schedule()이 호출됨)
        thread_block();
        // 3. 깨어나면 여기서 다시 시작 → while 재검사
    }
    sema->value--;

    intr_set_level(old_level);
}

void sema_up(struct semaphore *sema) {
    enum intr_level old_level = intr_disable();

    if (!list_empty(&sema->waiters)) {
        // waiters에서 하나 꺼내서 READY로
        struct thread *t = list_entry(
            list_pop_front(&sema->waiters),
            struct thread, elem);
        thread_unblock(t);
    }
    sema->value++;

    intr_set_level(old_level);
}
```

**핵심 포인트**
- `intr_disable()`로 atomic 보장
- `while` 루프 (not `if`): 깨어났을 때도 재검사 필요 (spurious wakeup, priority 경쟁)
- `thread_block()`은 schedule()을 호출 → 다른 스레드로 넘어감
- 깨어나는 시점은 `thread_unblock()`이 자신을 ready_list에 넣은 후

## 2.4 Lock: Semaphore + Ownership

Lock은 **value=1인 binary semaphore에 "소유자(holder)" 개념을 추가**한 것.

```c
struct lock {
    struct thread *holder;       // 누가 잡고 있는지
    struct semaphore semaphore;  // value=1로 초기화
};
```

**왜 holder가 필요한가?**
1. **재귀적 획득 방지**: 자기가 가진 lock을 또 잡으려 하면 deadlock → 검사 가능
2. **lock_release는 holder만 가능**: 다른 스레드가 풀면 안 됨
3. **Priority donation에 필수**: 누구에게 donation할지 알아야 함

```c
void lock_acquire(struct lock *lock) {
    ASSERT(!lock_held_by_current_thread(lock));
    sema_down(&lock->semaphore);
    lock->holder = thread_current();
}

void lock_release(struct lock *lock) {
    ASSERT(lock_held_by_current_thread(lock));
    lock->holder = NULL;
    sema_up(&lock->semaphore);
}
```

## 2.5 Condition Variable: 조건 기반 대기

Lock은 "들어갈 수 있나?"의 문제, CV는 "**조건이 만족됐나?**"의 문제.

**Monitor 패턴**:
```c
lock_acquire(&lock);
while (!조건) {
    cond_wait(&cv, &lock);  // lock 풀고 대기 → 깨어나면 다시 lock 획득
}
// 조건 만족 상태에서 작업
lock_release(&lock);
```

**왜 lock과 CV를 같이 써야 하나?**
"조건 검사 → 대기" 사이에 다른 스레드가 조건을 변경하면 wakeup signal을 놓칠 수 있음. CV의 wait은 "**lock 해제와 대기 진입을 atomic하게**" 처리해줍니다.

**Pintos 구현 핵심**
```c
void cond_wait(struct condition *cond, struct lock *lock) {
    struct semaphore_elem waiter;
    sema_init(&waiter.semaphore, 0);
    list_push_back(&cond->waiters, &waiter.elem);

    lock_release(lock);            // ① lock 풀고
    sema_down(&waiter.semaphore);  // ② 대기 (각 스레드마다 개인 세마포어)
    lock_acquire(lock);            // ③ 깨어나면 다시 lock 획득
}
```

**signal vs broadcast**
- `cond_signal`: 대기 중인 스레드 하나만 깨움
- `cond_broadcast`: 전부 깨움 (조건 만족 가능한 게 여럿일 때)

> **왜 `while`인가?** `cond_wait`에서 깨어났을 때, 다른 스레드가 먼저 lock을 잡아 조건을 다시 false로 만들 수 있음. 그래서 항상 재검사.

## 2.6 Deadlock: 4가지 필요조건

1. **Mutual Exclusion**: 자원이 한 번에 한 스레드만 사용 가능
2. **Hold and Wait**: 자원을 가진 채로 다른 자원을 기다림
3. **No Preemption**: 자원을 강제로 빼앗을 수 없음
4. **Circular Wait**: 대기 그래프에 사이클 존재

네 가지 모두 성립해야 deadlock 발생. 하나만 깨도 예방 가능.

**대표 예시: Dining Philosophers**
- 5명의 철학자가 원형 테이블, 사이마다 포크 1개
- 모두가 왼쪽 포크를 먼저 잡으면 → 모두가 오른쪽 포크 기다림 → deadlock
- 해결: 자원에 순서 부여 (낮은 번호 포크 먼저), 또는 한 명만 반대 순서로

---

# 3. 스케줄링: Priority와 Donation

## 3.1 기본 스케줄링 정책 복습

| 정책 | 동작 | 특징 |
|------|------|------|
| FCFS | 도착 순서대로 | 간단, 짧은 작업이 긴 작업 뒤에 막힘 (convoy effect) |
| SJF | 짧은 작업 먼저 | 평균 대기시간 최적, 실행시간 예측 어려움 |
| Round Robin | 시간 할당량(quantum)씩 순환 | 공평, quantum 크기가 핵심 |
| Priority | 높은 우선순위 먼저 | 직관적, **starvation/inversion 문제** |

Pintos 기본은 **Round Robin**이고, Project 1에서 **Priority Scheduling**으로 바꿔야 합니다.

## 3.2 Priority Scheduling 구현 포인트

Pintos 우선순위: **0 (PRI_MIN) ~ 63 (PRI_MAX)**, 기본값 31.

**구현 시 바꿔야 할 곳들** (놓치기 쉬운 포인트)

1. **`ready_list`에서 꺼낼 때**: 가장 높은 우선순위 선택
   ```c
   // next_thread_to_run() 또는 ready_list 삽입 시 정렬
   list_insert_ordered(&ready_list, &t->elem, cmp_priority, NULL);
   ```

2. **`thread_create()` 직후**: 새 스레드가 더 높으면 즉시 yield
   ```c
   if (new_thread->priority > thread_current()->priority)
       thread_yield();
   ```

3. **`thread_set_priority()`**: 자기 우선순위를 낮췄으면 yield 검사

4. **세마포어 대기열**: `sema_up`에서 깨울 때 가장 높은 우선순위부터
   ```c
   // sema->waiters를 우선순위로 정렬 후 pop
   list_sort(&sema->waiters, cmp_priority, NULL);
   ```

5. **CV 대기열**: `cond_signal`에서도 마찬가지로 정렬 필요

> **자주 놓치는 함정**: 대기 중인 스레드의 우선순위가 donation으로 **바뀔 수 있음**. 그래서 `sema_up` 시점에 매번 정렬해야 함 (삽입 시점 정렬만으로는 불충분).

## 3.3 Priority Inversion 문제

**시나리오**:
- 스레드 H(High), M(Medium), L(Low)
- L이 lock을 잡음
- H가 같은 lock을 요청 → BLOCKED
- M이 등장해서 CPU 점유 → L이 실행 못 함 → H도 영원히 대기

**결과**: 우선순위가 낮은 M이 H보다 먼저 실행되는 **역전 현상**.

**실제 사례**: NASA Mars Pathfinder (1997). 화성에서 시스템이 계속 리부트되던 원인이 바로 이것.

## 3.4 Priority Donation: 해법

**아이디어**: H가 L을 기다린다면, **H가 자기 우선순위를 L에게 빌려준다**. 그러면 L이 M보다 높은 우선순위가 되어 빨리 실행되고 lock을 풀어줌.

```
[Before donation]            [After donation]
H (pri=63) ─ wait ─┐         H (pri=63) ─ wait ─┐
                    │                              │
M (pri=31) running  │         L (pri=63 donated) running
                    │                              │
L (pri=1)  holds ───┘         M (pri=31) waiting
```

### 3.4.1 Single Donation

가장 단순한 케이스. 하나의 H가 하나의 L에게 donation.

```c
void lock_acquire(struct lock *lock) {
    struct thread *curr = thread_current();

    if (lock->holder != NULL) {
        curr->wait_on_lock = lock;
        // donation: holder의 donations 리스트에 자신을 추가
        list_insert_ordered(&lock->holder->donations,
                            &curr->donation_elem,
                            cmp_priority_donation, NULL);
        donate_priority();  // 우선순위 전파
    }

    sema_down(&lock->semaphore);

    curr->wait_on_lock = NULL;
    lock->holder = curr;
}
```

### 3.4.2 Multiple Donation

**한 스레드가 여러 lock을 가지고 있고, 각 lock마다 다른 스레드가 기다림**.

```
        L (holds lock1, lock2)
        ↑                ↑
    waits for         waits for
        │                │
    H1 (pri=50)     H2 (pri=60)
```

L의 우선순위 = max(원래 priority, H1.pri, H2.pri) = 60

**구현**:
- L의 `donations` 리스트에 H1, H2 모두 추가
- L이 lock 하나를 풀 때, 해당 lock을 기다리던 스레드만 donations에서 제거
- 남은 donations 중 최댓값으로 L의 priority 재계산

```c
void lock_release(struct lock *lock) {
    // 1. 이 lock을 기다리던 스레드들을 donations에서 제거
    remove_with_lock(lock);
    // 2. 남은 donations 기준으로 priority 재계산
    refresh_priority();

    lock->holder = NULL;
    sema_up(&lock->semaphore);
}

void remove_with_lock(struct lock *lock) {
    struct thread *curr = thread_current();
    struct list_elem *e;

    for (e = list_begin(&curr->donations);
         e != list_end(&curr->donations); ) {
        struct thread *t = list_entry(e, struct thread, donation_elem);
        if (t->wait_on_lock == lock)
            e = list_remove(e);
        else
            e = list_next(e);
    }
}

void refresh_priority(void) {
    struct thread *curr = thread_current();
    curr->priority = curr->init_priority;  // 원래 값으로 리셋

    if (!list_empty(&curr->donations)) {
        list_sort(&curr->donations, cmp_priority_donation, NULL);
        struct thread *front = list_entry(
            list_front(&curr->donations),
            struct thread, donation_elem);
        if (front->priority > curr->priority)
            curr->priority = front->priority;
    }
}
```

### 3.4.3 Nested Donation

**Donation의 연쇄 전파**.

```
H (pri=63)  →  waits for lock_A  held by  M (pri=31)
                                              ↓
                                   waits for lock_B  held by  L (pri=1)
```

H가 M에게 donation → M의 priority = 63
하지만 M도 L을 기다리고 있음 → M의 새 priority(63)를 다시 L에게 donation → L의 priority = 63

```c
void donate_priority(void) {
    struct thread *curr = thread_current();
    int depth = 0;
    const int MAX_DEPTH = 8;  // 무한 루프 방지

    while (curr->wait_on_lock != NULL && depth < MAX_DEPTH) {
        struct thread *holder = curr->wait_on_lock->holder;
        if (holder->priority >= curr->priority)
            break;  // 이미 충분히 높음
        holder->priority = curr->priority;
        curr = holder;  // 한 단계 위로 올라가서 또 전파
        depth++;
    }
}
```

> **왜 depth 제한?** 정상적인 코드에서는 lock 의존성이 깊지 않지만, 비정상적 상황(설계 오류 등)에서 무한 루프 방지. Pintos 표준은 8.

### 3.4.4 정리: 세 가지 케이스 비교

| 케이스 | 상황 | 핵심 처리 |
|--------|------|-----------|
| Single | H 1개 → L 1개 | donations 리스트에 추가 |
| Multiple | H 여러 개 → L 1개 (lock 여러 개) | lock 풀 때 해당 lock 대기자만 제거 |
| Nested | H → M → L (사슬) | wait_on_lock 따라가며 재귀적 전파 |

세 가지를 **분리해서 구현**하지 말고, 하나의 통합된 로직으로 처리하는 것이 깔끔합니다.


---

# 4. MLFQS와 Fixed-Point 연산

## 4.1 MLFQS의 기본 아이디어

**Multi-Level Feedback Queue Scheduler**: 우선순위가 **동적으로 변하는** 스케줄러.

**문제 의식**:
- Priority Scheduling은 사용자가 우선순위를 직접 지정 → starvation 가능
- 어떤 작업이 I/O bound인지 CPU bound인지 미리 알 수 없음
- → 스케줄러가 **관찰을 통해 자동 조정**하자

**핵심 휴리스틱**:
- CPU를 많이 쓴 스레드는 우선순위를 낮춤 (더 양보하라)
- 최근에 못 쓴 스레드는 우선순위를 높임 (이제 줄게)

## 4.2 핵심 변수 3가지

### nice
- 스레드의 "양보 정도". -20 ~ +20, 기본값 0
- 높을수록 "착함" = 우선순위 양보 (낮은 priority)

### recent_cpu
- 최근에 CPU를 얼마나 사용했는지 (지수 감쇠 평균)
- 매 timer tick마다 현재 실행 중인 스레드의 값을 1 증가
- 매 1초(TIMER_FREQ tick)마다 모든 스레드에 대해 감쇠 적용:

  ```
  recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
  ```

### load_avg
- 시스템 전체 부하 (지수 가중 이동 평균)
- 매 1초마다 갱신:

  ```
  load_avg = (59/60) * load_avg + (1/60) * ready_threads
  ```
  
  (`ready_threads` = ready_list에 있는 스레드 수 + 현재 실행 중인 스레드. idle 제외)

### priority 재계산
**4 tick마다 모든 스레드에 대해**:

```
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
```

PRI_MIN(0) ~ PRI_MAX(63) 범위로 클램핑.

## 4.3 Fixed-Point 연산이 왜 필요한가

**문제**: 위 수식들에 `59/60` 같은 분수와 곱셈이 잔뜩 들어감. 부동소수점이 자연스러움. **그런데 Pintos 커널은 부동소수점을 못 씀** (FPU 컨텍스트를 스레드마다 저장/복원해야 하는 부담 때문).

**해결**: 정수로 소수를 표현하는 **고정소수점(fixed-point) 표기법**.

### 17.14 포맷
32비트 정수의 비트를 이렇게 해석:
- 부호 1비트
- 정수부 17비트
- 소수부 14비트

즉, 실수 X를 정수 X × 2^14로 표현.

```
실수 1.0    → 정수 1 * 16384 = 16384
실수 0.5    → 정수 0.5 * 16384 = 8192
실수 -2.25  → 정수 -2.25 * 16384 = -36864
```

### 변환과 연산 (f = 2^14 = 16384)

```c
#define F (1 << 14)  // 16384

// 정수 ↔ fixed-point
int n_to_fp(int n)        { return n * F; }
int fp_to_n_zero(int x)   { return x / F; }                // 0방향 truncate
int fp_to_n_round(int x) {                                  // 반올림
    return x >= 0 ? (x + F/2) / F : (x - F/2) / F;
}

// 덧셈/뺄셈: fixed-point끼리는 그냥 더하면 됨
int add_fp(int x, int y)  { return x + y; }
int sub_fp(int x, int y)  { return x - y; }

// fixed-point + 정수: 정수를 변환해서 더함
int add_mixed(int x, int n) { return x + n * F; }
int sub_mixed(int x, int n) { return x - n * F; }

// 곱셈: 두 fixed-point를 곱하면 (X*F) * (Y*F) = X*Y*F^2 → F로 나눠야 함
int mul_fp(int x, int y) { return ((int64_t)x) * y / F; }

// fixed-point * 정수: 그냥 곱함 (정수는 F가 안 곱해진 상태)
int mul_mixed(int x, int n) { return x * n; }

// 나눗셈: (X*F) / (Y*F) = X/Y → F를 곱해서 보정
int div_fp(int x, int y) { return ((int64_t)x) * F / y; }

// fixed-point / 정수
int div_mixed(int x, int n) { return x / n; }
```

> **왜 `int64_t` 캐스팅?** `x * y`에서 둘 다 큰 값이면 32비트 오버플로우. 곱셈/나눗셈에서만 64비트로 임시 확장.

## 4.4 MLFQS 구현 흐름

**`thread.c`의 `thread_tick()` 안에서**:

```c
void thread_tick(void) {
    struct thread *t = thread_current();

    if (thread_mlfqs) {
        // 1. 매 tick: 현재 스레드의 recent_cpu 증가 (idle 제외)
        if (t != idle_thread)
            t->recent_cpu = add_mixed(t->recent_cpu, 1);

        // 2. 매 1초 (TIMER_FREQ tick): load_avg와 모든 recent_cpu 갱신
        if (timer_ticks() % TIMER_FREQ == 0) {
            mlfqs_calculate_load_avg();
            mlfqs_recalculate_recent_cpu();  // 모든 스레드 대상
        }

        // 3. 매 4 tick: 모든 스레드의 priority 재계산
        if (timer_ticks() % 4 == 0) {
            mlfqs_recalculate_priority();
        }
    }
    // ... timer interrupt 끝
}
```

**`mlfqs_calculate_load_avg()`**:
```c
void mlfqs_calculate_load_avg(void) {
    int ready_count = list_size(&ready_list);
    if (thread_current() != idle_thread)
        ready_count++;

    // load_avg = (59/60) * load_avg + (1/60) * ready_count
    int term1 = mul_fp(div_fp(n_to_fp(59), n_to_fp(60)), load_avg);
    int term2 = mul_mixed(div_fp(n_to_fp(1), n_to_fp(60)), ready_count);
    load_avg = add_fp(term1, term2);
}
```

**`mlfqs_calculate_priority(t)`**:
```c
void mlfqs_calculate_priority(struct thread *t) {
    if (t == idle_thread) return;

    // priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
    int term1 = div_mixed(t->recent_cpu, 4);
    int term2 = 2 * t->nice;
    int pri = fp_to_n_zero(sub_mixed(sub_mixed(n_to_fp(PRI_MAX), 
                                                 fp_to_n_zero(term1)),
                                       term2));
    if (pri > PRI_MAX) pri = PRI_MAX;
    if (pri < PRI_MIN) pri = PRI_MIN;
    t->priority = pri;
}
```

## 4.5 MLFQS와 Donation의 관계

**중요**: MLFQS가 활성화되면(`thread_mlfqs == true`) **priority donation은 비활성화**됩니다.

이유:
- MLFQS에서 priority는 시스템이 자동 계산
- donation으로 임의로 바꾸면 MLFQS의 가정이 깨짐
- `thread_set_priority()`도 무시됨

```c
void thread_set_priority(int new_priority) {
    if (thread_mlfqs) return;  // MLFQS면 무시

    thread_current()->init_priority = new_priority;
    refresh_priority();
    // ... yield 검사
}
```


---

# 5. 통합 정리

## 5.1 권장 구현 순서

Pintos Project 1을 진행한다면 이 순서로:

### Step 1: Alarm Clock (busy wait 제거)
**문제**: 기존 `timer_sleep()`은 busy wait
```c
// BAD: CPU 낭비
void timer_sleep(int64_t ticks) {
    int64_t start = timer_ticks();
    while (timer_elapsed(start) < ticks)
        thread_yield();
}
```

**해결**: sleep_list에 넣고 BLOCK, 매 tick마다 깨울 시간 확인
```c
void timer_sleep(int64_t ticks) {
    int64_t wakeup = timer_ticks() + ticks;
    thread_sleep(wakeup);  // 직접 구현: BLOCKED + sleep_list에 추가
}

// timer_interrupt에서
void timer_interrupt(...) {
    ticks++;
    thread_awake(ticks);  // 깨울 시간 된 스레드들 unblock
}
```

> 이 단계를 통해 **thread_block / thread_unblock**의 사용법을 익히게 됨. 이후 모든 동기화의 토대.

### Step 2: Priority Scheduling
- `ready_list`를 우선순위 정렬 유지
- `thread_create`, `thread_set_priority`, `sema_up`, `cond_signal` 등에서 yield/정렬 추가

### Step 3: Priority Donation
- single → multiple → nested 순서로 케이스 추가
- `lock_acquire`, `lock_release`에 donation 로직

### Step 4: MLFQS
- fixed-point 산술 헤더 먼저 작성 (`fixed_point.h`)
- `thread_tick()`에서 주기적 갱신 호출
- `thread_mlfqs` 플래그로 기존 priority 로직과 분기

## 5.2 자주 빠지는 함정 체크리스트

- [ ] `sema_up`에서 waiters 정렬 후 pop (priority가 도중에 바뀌었을 수 있음)
- [ ] `cond_signal`도 마찬가지로 semaphore_elem들의 우선순위 비교
- [ ] `thread_set_priority`는 donation 받은 상태면 init_priority만 바꾸기
- [ ] nested donation depth 제한 (보통 8)
- [ ] MLFQS에서 idle thread는 모든 계산에서 제외
- [ ] fixed-point 곱셈/나눗셈에서 `int64_t` 캐스팅
- [ ] `intr_disable()`로 보호된 영역에서 `thread_block()` 호출 가능 여부 확인
- [ ] `thread_create()` 후 새 스레드가 더 높은 우선순위면 즉시 yield

## 5.3 핵심 개념 한 줄 요약

| 개념 | 한 줄 정의 |
|------|------------|
| Thread | 실행의 단위. context(레지스터 + 스택)로 식별 |
| Context Switch | 한 스레드의 레지스터 상태를 저장하고 다른 것으로 교체 |
| Interrupt | CPU 실행 흐름을 강제로 가로채는 신호 |
| Race Condition | 실행 순서에 따라 결과가 달라지는 버그 |
| Semaphore | 카운터 기반 동기화의 가장 기본 형태 |
| Lock | binary semaphore + ownership |
| Condition Variable | 조건 만족까지 lock 풀고 대기하는 도구 |
| Priority Inversion | 낮은 우선순위가 높은 우선순위를 막는 현상 |
| Priority Donation | 기다리는 스레드가 자기 priority를 빌려주어 inversion 해소 |
| MLFQS | CPU 사용량을 관찰해 priority를 자동 조정 |
| Fixed-Point | 부동소수점 없이 정수로 소수 표현 (17.14) |

## 5.4 학습 자가 점검 질문

각 섹션을 끝낸 뒤 답해보세요. 막히면 그 부분 다시 봐야 합니다.

**Section 1 (토대)**
- 스레드 상태 4가지와 전이 함수들을 그림으로 그릴 수 있는가?
- 컨텍스트 스위칭이 왜 어셈블리로 작성되는지 설명할 수 있는가?
- `intr_context()`가 true일 때 하지 말아야 할 일은?

**Section 2 (동기화)**
- `sema_down`의 `while` 루프가 `if`이면 안 되는 이유는?
- Lock과 Semaphore의 차이를 3가지 이상 말할 수 있는가?
- CV에서 wait 시 lock을 같이 넘기는 이유는?

**Section 3 (스케줄링)**
- Multiple donation과 Nested donation을 한 시나리오로 설명할 수 있는가?
- `lock_release` 시 donations에서 누구를 제거해야 하는가?
- Priority inversion이 실제로 일어났던 사건은?

**Section 4 (MLFQS)**
- 17.14 포맷에서 `1.5`는 어떤 정수로 표현되는가? (답: 24576)
- `recent_cpu`가 시간이 지나며 감쇠하는 메커니즘은?
- MLFQS에서 priority donation이 비활성화되는 이유는?

---

## 부록: 추가 참고 자료

- **Pintos 공식 문서**: Stanford CS140의 원본 문서가 가장 정확
- **OSTEP** (Operating Systems: Three Easy Pieces) - Concurrency 파트
- **소스 직독**: `threads/thread.c`, `threads/synch.c`, `devices/timer.c`는 분량이 적어 직접 읽는 게 빠름
- 디버깅 시 `printf`보다 **`ASSERT`와 `intr_dump_frame`** 활용
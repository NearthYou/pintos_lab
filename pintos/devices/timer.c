#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* 8254 타이머 칩의 하드웨어 세부사항은 [8254]을 참조하십시오. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* OS 부팅 이후의 타이머 틱 수. */
static int64_t ticks;

/* 타이머 틱당 루프 수.
   timer_calibrate()에서 초기화한다. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* TODO: sleeping thread들을 저장할 sleep_list를 선언한다. */


/* TODO: thread의 wakeup_tick을 기준으로 sleep_list 정렬에 사용할 비교 helper함수를 추가한다. */
// helper(a,b) -> a가 가리키는 스레드와 b가 가리키는 스레드의 wakeup_tick을 비교해서 정렬

/* 8254 프로그래머블 인터벌 타이머(PIT)가 초당 PIT_FREQ번
   인터럽트를 발생시키도록 설정하고 해당 인터럽트를 등록한다. */
void
timer_init (void) {
	/* 8254 입력 주파수를 TIMER_FREQ로 나누고 가장 가까운 값으로 반올림한다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: 카운터 0, LSB 다음 MSB, 모드 2, 바이너리. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* 짧은 지연을 구현하는 데 사용되는 loops_per_tick을 보정합니다. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* 가장 큰 2의 거듭제곱으로 loops_per_tick을 대략적으로 계산합니다.
	   여전히 타이머 틱이 1개 미만입니다. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* loops_per_tick의 다음 8비트를 구체화합니다. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* OS가 부팅된 이후의 타이머 틱 수를 반환한다. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* THEN 이후 경과한 타이머 틱 수를 반환한다.
   THEN은 이전에 timer_ticks()가 반환한 값이어야 한다. */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

// 	현재 timer_sleep은 busy wait 상태인데, 이 방식은 thread가 계속 CPU를 양보했다가 다시 실행되면서 시간을 확인하므로 CPU 비용이 낭비된다.




// 흐름적으로 timer_sleep()는 현재 thread를 “언제 깨울지 기록하고 BLOCKED로 재우는 함수”이고, 
// timer_interrupt()는 매 tick마다 시간을 올리면서 “이제 깨울 시간이 된 thread를 READY로 되돌리는 함수”다.



/* 약 TICKS 타이머 틱 동안 실행을 일시 중지합니다. */
void
timer_sleep (int64_t ticks) {
	

	/* 기존처럼 busy waiting을 위해 while 루프를 도는 대신,
	
	이때 thread_sleep()에는 깨어나야 할 alarm time을 전달합니다. 이 값은 start plus ticks, 즉 시작 시각에 ticks를 더한 값입니다.*/
	if (ticks <= 0)
		return;
	
	int64_t start = timer_ticks ();

	ASSERT (intr_get_level () == INTR_ON);

	thread_sleep(start + ticks);
	
		// 그래서 timer_sleep을 호출한 thread는 먼저 자신의 wakeup_tick을 계산한다.

		// 그다음 현재 thread를 sleep_list에 넣고 BLOCKED 상태로 만든다.
		
		// sleep_list에서 wakeup_tick을 가지고있는 자고있는 스레드들을 오름차순으로 정렬한다.
		
		/* TODO: ticks <= 0 이면 즉시 반환한다. */
		/* TODO: 현재 thread의 wakeup_tick을 현재 tick 기준으로 계산한다. */
		/* TODO: 삽입과 block 과정이 timer interrupt와 엇갈리지 않도록 interrupt를 잠시 비활성화한다. */
		/* TODO: sleep_list가 wakeup_tick 오름차순을 유지하도록 현재 thread를 삽입한다. */
		/* TODO: 현재 thread를 block 상태로 만든다. */
		/* TODO: block 이후 interrupt 상태를 적절히 복구한다. */

}

/* 약 MS밀리초 동안 실행을 일시 중지합니다. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* 약 US 마이크로초 동안 실행을 일시 중지합니다. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* 약 NS 나노초 동안 실행을 일시 중지합니다. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* 타이머 통계를 인쇄합니다. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* 타이머 인터럽트 핸들러. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();
	thread_wake (ticks);
		// 이후 timer interrupt가 발생할 때마다 ticks 값이 증가하고, sleep_list를 확인한다.

		// 현재 tick이 어떤 thread의 wakeup_tick보다 같거나 커지면, 그 thread는 더 이상 잘 필요가 없으므로 block을 해제한다.

		// block이 해제된 thread는 바로 RUNNING이 되는 것이 아니라 ready_list로 이동한다.

		/* TODO: sleep_list를 앞에서부터 확인한다. */
		/* TODO: wakeup_tick <= ticks 인 sleeping thread들을 모두 깨운다. */
		/* TODO: 깨울 때는 thread_unblock()을 사용하여 READY 상태로 되돌린다. */
		/* TODO: 아직 일어날 시간이 되지 않은 thread가 나오면 정렬된 sleep_list 특성을 이용해 확인을 멈춘다 */


}

/* LOOPS번 반복하는 동안 타이머 틱이 하나 넘게 지나면 true,
   그렇지 않으면 false를 반환한다. */
static bool
too_many_loops (unsigned loops) {
	/* 타이머 틱을 기다립니다. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* LOOPS 루프를 실행합니다. */
	start = ticks;
	busy_wait (loops);

	/* 틱 수가 변경되면 너무 오랫동안 반복한 것입니다. */
	barrier ();
	return start != ticks;
}

/* 짧은 지연을 구현하기 위해 단순 루프를 LOOPS번 반복한다.

   코드 정렬은 타이밍에 큰 영향을 줄 수 있다. 이 함수가 호출 위치마다
   다르게 인라인되면 결과를 예측하기 어려워지므로 NO_INLINE으로 표시한다. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* 약 NUM / DENOM초 동안 절전 모드로 전환됩니다. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* NUM / DENOM 초를 타이머 틱으로 변환하고 반내림합니다.

	   (NUM / DENOM) 초
	   ---------------------- = NUM * TIMER_FREQ / DENOM 틱.
	   1초 / TIMER_FREQ 틱
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* 적어도 한 번의 전체 타이머 틱을 기다린다.
		   timer_sleep()은 CPU를 다른 프로세스에 양보하므로 이를 사용한다. */
		timer_sleep (ticks);
	} else {
		/* 그렇지 않으면 더 정확한 서브틱 타이밍을 위해 바쁜 대기 루프를 사용한다.
		   오버플로 가능성을 피하려고 분자와 분모를 1000으로 줄인다. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}

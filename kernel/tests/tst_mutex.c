#include <common/lock.h>
#include <common/kprint.h>
#include <arch/machine/smp.h>
#include <common/macro.h>
#include <mm/kmalloc.h>
#include <arch/machine/pmu.h>

#include "tests.h"

#define LOCK_TEST_NUM           100000000

volatile int mutex_start_flag = 0;
volatile int mutex_finish_flag = 0;

/* Mutex test count */
unsigned long mutex_test_count = 0;

void tst_mutex(void)
{
	u64 start = 0, end = 0;

        /* Init */
	if (smp_get_cpu_id() == 0) {
		start = pmu_read_real_cycle();
	}

	/* ============ Start Barrier ============ */
	lock(&big_kernel_lock);
	mutex_start_flag++;
	unlock(&big_kernel_lock);
	while (mutex_start_flag != PLAT_CPU_NUM) ;
	/* ============ Start Barrier ============ */

	/* Mutex Lock */
	for (int i = 0; i < LOCK_TEST_NUM; i++) {
		if (i % 2)
			while (try_lock(&big_kernel_lock) != 0) ;
		else
			lock(&big_kernel_lock);
		/* Critical Section */
		mutex_test_count++;
		unlock(&big_kernel_lock);
	}

        /* ============ Finish Barrier ============ */
	lock(&big_kernel_lock);
	mutex_finish_flag++;
	unlock(&big_kernel_lock);
	while (mutex_finish_flag != PLAT_CPU_NUM) ;
	/* ============ Finish Barrier ============ */

	/* Check */
	BUG_ON(mutex_test_count != PLAT_CPU_NUM * LOCK_TEST_NUM);

	if (smp_get_cpu_id() == 0) {
		end = pmu_read_real_cycle();
		kinfo("[TEST] LOCK Performance %ld\n", (end-start)/(LOCK_TEST_NUM * PLAT_CPU_NUM));
	}

}
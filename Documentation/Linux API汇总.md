# Linux API汇总

## 1.The ALSA Driver API


## 2.linux Kernel Crypto API


## 3.Linux Kernel Crypto API


## 4.Linux Device Drivers
### 4.1 Driver Basics

#### 4.1.1 Driver Entry and Exit points
#### 4.1.2 Atomic and pointer manipulation
#### 4.1.3 Delaying, scheduling, and timer routines

struct prev_cputime
snaphsot of system and user cputime

Definition

struct prev_cputime {
#ifndef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
  cputime_t utime;
  cputime_t stime;
  raw_spinlock_t lock;
#endif
};
Members

cputime_t utime
time spent in user mode
cputime_t stime
time spent in system mode
raw_spinlock_t lock
protects the above two fields
Description

Stores previous user/system time values such that we can guarantee monotonicity.

struct task_cputime
collected CPU time counts

Definition

struct task_cputime {
  cputime_t utime;
  cputime_t stime;
  unsigned long long sum_exec_runtime;
};
Members

cputime_t utime
time spent in user mode, in cputime_t units
cputime_t stime
time spent in kernel mode, in cputime_t units
unsigned long long sum_exec_runtime
total time spent on the CPU, in nanoseconds
Description

This structure groups together three kinds of CPU time that are tracked for threads and thread groups. Most things considering CPU time want to group these counts together and treat all three of them in parallel.

struct thread_group_cputimer
thread group interval timer counts

Definition

struct thread_group_cputimer {
  struct task_cputime_atomic cputime_atomic;
  bool running;
  bool checking_timer;
};
Members

struct task_cputime_atomic cputime_atomic
atomic thread group interval timers.
bool running
true when there are timers running and cputime_atomic receives updates.
bool checking_timer
true when a thread in the group is in the process of checking for thread group timers.
Description

This structure contains the version of task_cputime, above, that is used for thread group CPU timer calculations.

int pid_alive(const struct task_struct * p)
check that a task structure is not stale

Parameters

const struct task_struct * p
Task structure to be checked.
Description

Test if a process is not yet dead (at most zombie state) If pid_alive fails, then pointers within the task structure can be stale and must not be dereferenced.

Return

1 if the process is alive. 0 otherwise.

int is_global_init(struct task_struct * tsk)
check if a task structure is init. Since init is free to have sub-threads we need to check tgid.

Parameters

struct task_struct * tsk
Task structure to be checked.
Description

Check if a task structure is the first user space task the kernel created.

Return

1 if the task structure is init. 0 otherwise.

int task_nice(const struct task_struct * p)
return the nice value of a given task.

Parameters

const struct task_struct * p
the task in question.
Return

The nice value [ -20 ... 0 ... 19 ].

bool is_idle_task(const struct task_struct * p)
is the specified task an idle task?

Parameters

const struct task_struct * p
the task in question.
Return

1 if p is an idle task. 0 otherwise.

void threadgroup_change_begin(struct task_struct * tsk)
mark the beginning of changes to a threadgroup

Parameters

struct task_struct * tsk
task causing the changes
Description

All operations which modify a threadgroup - a new thread joining the group, death of a member thread (the assertion of PF_EXITING) and exec(2) dethreading the process and replacing the leader - are wrapped by threadgroup_change_{begin|end}(). This is to provide a place which subsystems needing threadgroup stability can hook into for synchronization.

void threadgroup_change_end(struct task_struct * tsk)
mark the end of changes to a threadgroup

Parameters

struct task_struct * tsk
task causing the changes
Description

See threadgroup_change_begin().

int wake_up_process(struct task_struct * p)
Wake up a specific process

Parameters

struct task_struct * p
The process to be woken up.
Description

Attempt to wake up the nominated process and move it to the set of runnable processes.

Return

1 if the process was woken up, 0 if it was already running.

It may be assumed that this function implies a write memory barrier before changing the task state if and only if any tasks are woken up.

void preempt_notifier_register(struct preempt_notifier * notifier)
tell me when current is being preempted & rescheduled

Parameters

struct preempt_notifier * notifier
notifier struct to register
void preempt_notifier_unregister(struct preempt_notifier * notifier)
no longer interested in preemption notifications

Parameters

struct preempt_notifier * notifier
notifier struct to unregister
Description

This is not safe to call from within a preemption notifier.

__visible void __sched notrace preempt_schedule_notrace(void)
preempt_schedule called by tracing

Parameters

void
no arguments
Description

The tracing infrastructure uses preempt_enable_notrace to prevent recursion and tracing preempt enabling caused by the tracing infrastructure itself. But as tracing can happen in areas coming from userspace or just about to enter userspace, a preempt enable can occur before user_exit() is called. This will cause the scheduler to be called when the system is still in usermode.

To prevent this, the preempt_enable_notrace will use this function instead of preempt_schedule() to exit user context if needed before calling the scheduler.

int sched_setscheduler(struct task_struct * p, int policy, const struct sched_param * param)
change the scheduling policy and/or RT priority of a thread.

Parameters

struct task_struct * p
the task in question.
int policy
new policy.
const struct sched_param * param
structure containing the new RT priority.
Return

0 on success. An error code otherwise.

NOTE that the task may be already dead.

int sched_setscheduler_nocheck(struct task_struct * p, int policy, const struct sched_param * param)
change the scheduling policy and/or RT priority of a thread from kernelspace.

Parameters

struct task_struct * p
the task in question.
int policy
new policy.
const struct sched_param * param
structure containing the new RT priority.
Description

Just like sched_setscheduler, only don’t bother checking if the current context has permission. For example, this is needed in stop_machine(): we create temporary high priority worker threads, but our caller might not have that capability.

Return

0 on success. An error code otherwise.

void __sched yield(void)
yield the current processor to other threads.

Parameters

void
no arguments
Description

Do not ever use this function, there’s a 99% chance you’re doing it wrong.

The scheduler is at all times free to pick the calling task as the most eligible task to run, if removing the yield() call from your code breaks it, its already broken.

Typical broken usage is:

while (!event)
yield();
where one assumes that yield() will let ‘the other’ process run that will make event true. If the current task is a SCHED_FIFO task that will never happen. Never use yield() as a progress guarantee!!

If you want to use yield() to wait for something, use wait_event(). If you want to use yield() to be ‘nice’ for others, use cond_resched(). If you still want to use yield(), do not!

int __sched yield_to(struct task_struct * p, bool preempt)
yield the current processor to another thread in your thread group, or accelerate that thread toward the processor it’s on.

Parameters

struct task_struct * p
target task
bool preempt
whether task preemption is allowed or not
Description

It’s the caller’s job to ensure that the target task struct can’t go away on us before we can do any checks.

Return

true (>0) if we indeed boosted the target task. false (0) if we failed to boost the target. -ESRCH if there’s no task to yield to.
int cpupri_find(struct cpupri * cp, struct task_struct * p, struct cpumask * lowest_mask)
find the best (lowest-pri) CPU in the system

Parameters

struct cpupri * cp
The cpupri context
struct task_struct * p
The task
struct cpumask * lowest_mask
A mask to fill in with selected CPUs (or NULL)
Note

This function returns the recommended CPUs as calculated during the current invocation. By the time the call returns, the CPUs may have in fact changed priorities any number of times. While not ideal, it is not an issue of correctness since the normal rebalancer logic will correct any discrepancies created by racing against the uncertainty of the current priority configuration.

Return

(int)bool - CPUs were found

void cpupri_set(struct cpupri * cp, int cpu, int newpri)
update the cpu priority setting

Parameters

struct cpupri * cp
The cpupri context
int cpu
The target cpu
int newpri
The priority (INVALID-RT99) to assign to this CPU
Note

Assumes cpu_rq(cpu)->lock is locked

Return

(void)

int cpupri_init(struct cpupri * cp)
initialize the cpupri structure

Parameters

struct cpupri * cp
The cpupri context
Return

-ENOMEM on memory allocation failure.

void cpupri_cleanup(struct cpupri * cp)
clean up the cpupri structure

Parameters

struct cpupri * cp
The cpupri context
void cpu_load_update(struct rq * this_rq, unsigned long this_load, unsigned long pending_updates)
update the rq->cpu_load[] statistics

Parameters

struct rq * this_rq
The rq to update statistics for
unsigned long this_load
The current load
unsigned long pending_updates
The number of missed updates
Description

Update rq->cpu_load[] statistics. This function is usually called every scheduler tick (TICK_NSEC).

This function computes a decaying average:

load[i]’ = (1 - 1/2^i) * load[i] + (1/2^i) * load
Because of NOHZ it might not get called on every tick which gives need for the pending_updates argument.

load[i]_n = (1 - 1/2^i) * load[i]_n-1 + (1/2^i) * load_n-1
= A * load[i]_n-1 + B ; A := (1 - 1/2^i), B := (1/2^i) * load = A * (A * load[i]_n-2 + B) + B = A * (A * (A * load[i]_n-3 + B) + B) + B = A^3 * load[i]_n-3 + (A^2 + A + 1) * B = A^n * load[i]_0 + (A^(n-1) + A^(n-2) + ... + 1) * B = A^n * load[i]_0 + ((1 - A^n) / (1 - A)) * B = (1 - 1/2^i)^n * (load[i]_0 - load) + load
In the above we’ve assumed load_n := load, which is true for NOHZ_FULL as any change in load would have resulted in the tick being turned back on.

For regular NOHZ, this reduces to:

load[i]_n = (1 - 1/2^i)^n * load[i]_0
see decay_load_misses(). For NOHZ_FULL we get to subtract and add the extra term.

int get_sd_load_idx(struct sched_domain * sd, enum cpu_idle_type idle)
Obtain the load index for a given sched domain.

Parameters

struct sched_domain * sd
The sched_domain whose load_idx is to be obtained.
enum cpu_idle_type idle
The idle status of the CPU for whose sd load_idx is obtained.
Return

The load index.

void update_sg_lb_stats(struct lb_env * env, struct sched_group * group, int load_idx, int local_group, struct sg_lb_stats * sgs, bool * overload)
Update sched_group’s statistics for load balancing.

Parameters

struct lb_env * env
The load balancing environment.
struct sched_group * group
sched_group whose statistics are to be updated.
int load_idx
Load index of sched_domain of this_cpu for load calc.
int local_group
Does group contain this_cpu.
struct sg_lb_stats * sgs
variable to hold the statistics for this group.
bool * overload
Indicate more than one runnable task for any CPU.
bool update_sd_pick_busiest(struct lb_env * env, struct sd_lb_stats * sds, struct sched_group * sg, struct sg_lb_stats * sgs)
return 1 on busiest group

Parameters

struct lb_env * env
The load balancing environment.
struct sd_lb_stats * sds
sched_domain statistics
struct sched_group * sg
sched_group candidate to be checked for being the busiest
struct sg_lb_stats * sgs
sched_group statistics
Description

Determine if sg is a busier group than the previously selected busiest group.

Return

true if sg is a busier group than the previously selected busiest group. false otherwise.

void update_sd_lb_stats(struct lb_env * env, struct sd_lb_stats * sds)
Update sched_domain’s statistics for load balancing.

Parameters

struct lb_env * env
The load balancing environment.
struct sd_lb_stats * sds
variable to hold the statistics for this sched_domain.
int check_asym_packing(struct lb_env * env, struct sd_lb_stats * sds)
Check to see if the group is packed into the sched doman.

Parameters

struct lb_env * env
The load balancing environment.
struct sd_lb_stats * sds
Statistics of the sched_domain which is to be packed
Description

This is primarily intended to used at the sibling level. Some cores like POWER7 prefer to use lower numbered SMT threads. In the case of POWER7, it can move to lower SMT modes only when higher threads are idle. When in lower SMT modes, the threads will perform better since they share less core resources. Hence when we have idle threads, we want them to be the higher ones.

This packing function is run on idle threads. It checks to see if the busiest CPU in this domain (core in the P7 case) has a higher CPU number than the packing function is being run on. Here we are assuming lower CPU number will be equivalent to lower a SMT thread number.

Return

1 when packing is required and a task should be moved to this CPU. The amount of the imbalance is returned in *imbalance.

void fix_small_imbalance(struct lb_env * env, struct sd_lb_stats * sds)
Calculate the minor imbalance that exists amongst the groups of a sched_domain, during load balancing.

Parameters

struct lb_env * env
The load balancing environment.
struct sd_lb_stats * sds
Statistics of the sched_domain whose imbalance is to be calculated.
void calculate_imbalance(struct lb_env * env, struct sd_lb_stats * sds)
Calculate the amount of imbalance present within the groups of a given sched_domain during load balance.

Parameters

struct lb_env * env
load balance environment
struct sd_lb_stats * sds
statistics of the sched_domain whose imbalance is to be calculated.
struct sched_group * find_busiest_group(struct lb_env * env)
Returns the busiest group within the sched_domain if there is an imbalance.

Parameters

struct lb_env * env
The load balancing environment.
Description

Also calculates the amount of weighted load which should be moved to restore balance.

Return

The busiest group if imbalance exists.
DECLARE_COMPLETION(work)
declare and initialize a completion structure

Parameters

work
identifier for the completion structure
Description

This macro declares and initializes a completion structure. Generally used for static declarations. You should use the _ONSTACK variant for automatic variables.

DECLARE_COMPLETION_ONSTACK(work)
declare and initialize a completion structure

Parameters

work
identifier for the completion structure
Description

This macro declares and initializes a completion structure on the kernel stack.

void init_completion(struct completion * x)
Initialize a dynamically allocated completion

Parameters

struct completion * x
pointer to completion structure that is to be initialized
Description

This inline function will initialize a dynamically created completion structure.

void reinit_completion(struct completion * x)
reinitialize a completion structure

Parameters

struct completion * x
pointer to completion structure that is to be reinitialized
Description

This inline function should be used to reinitialize a completion structure so it can be reused. This is especially important after complete_all() is used.

unsigned long __round_jiffies(unsigned long j, int cpu)
function to round jiffies to a full second

Parameters

unsigned long j
the time in (absolute) jiffies that should be rounded
int cpu
the processor number on which the timeout will happen
Description

__round_jiffies() rounds an absolute time in the future (in jiffies) up or down to (approximately) full seconds. This is useful for timers for which the exact time they fire does not matter too much, as long as they fire approximately every X seconds.

By rounding these timers to whole seconds, all such timers will fire at the same time, rather than at various times spread out. The goal of this is to have the CPU wake up less, which saves power.

The exact rounding is skewed for each processor to avoid all processors firing at the exact same time, which could lead to lock contention or spurious cache line bouncing.

The return value is the rounded version of the j parameter.

unsigned long __round_jiffies_relative(unsigned long j, int cpu)
function to round jiffies to a full second

Parameters

unsigned long j
the time in (relative) jiffies that should be rounded
int cpu
the processor number on which the timeout will happen
Description

__round_jiffies_relative() rounds a time delta in the future (in jiffies) up or down to (approximately) full seconds. This is useful for timers for which the exact time they fire does not matter too much, as long as they fire approximately every X seconds.

By rounding these timers to whole seconds, all such timers will fire at the same time, rather than at various times spread out. The goal of this is to have the CPU wake up less, which saves power.

The exact rounding is skewed for each processor to avoid all processors firing at the exact same time, which could lead to lock contention or spurious cache line bouncing.

The return value is the rounded version of the j parameter.

unsigned long round_jiffies(unsigned long j)
function to round jiffies to a full second

Parameters

unsigned long j
the time in (absolute) jiffies that should be rounded
Description

round_jiffies() rounds an absolute time in the future (in jiffies) up or down to (approximately) full seconds. This is useful for timers for which the exact time they fire does not matter too much, as long as they fire approximately every X seconds.

By rounding these timers to whole seconds, all such timers will fire at the same time, rather than at various times spread out. The goal of this is to have the CPU wake up less, which saves power.

The return value is the rounded version of the j parameter.

unsigned long round_jiffies_relative(unsigned long j)
function to round jiffies to a full second

Parameters

unsigned long j
the time in (relative) jiffies that should be rounded
Description

round_jiffies_relative() rounds a time delta in the future (in jiffies) up or down to (approximately) full seconds. This is useful for timers for which the exact time they fire does not matter too much, as long as they fire approximately every X seconds.

By rounding these timers to whole seconds, all such timers will fire at the same time, rather than at various times spread out. The goal of this is to have the CPU wake up less, which saves power.

The return value is the rounded version of the j parameter.

unsigned long __round_jiffies_up(unsigned long j, int cpu)
function to round jiffies up to a full second

Parameters

unsigned long j
the time in (absolute) jiffies that should be rounded
int cpu
the processor number on which the timeout will happen
Description

This is the same as __round_jiffies() except that it will never round down. This is useful for timeouts for which the exact time of firing does not matter too much, as long as they don’t fire too early.

unsigned long __round_jiffies_up_relative(unsigned long j, int cpu)
function to round jiffies up to a full second

Parameters

unsigned long j
the time in (relative) jiffies that should be rounded
int cpu
the processor number on which the timeout will happen
Description

This is the same as __round_jiffies_relative() except that it will never round down. This is useful for timeouts for which the exact time of firing does not matter too much, as long as they don’t fire too early.

unsigned long round_jiffies_up(unsigned long j)
function to round jiffies up to a full second

Parameters

unsigned long j
the time in (absolute) jiffies that should be rounded
Description

This is the same as round_jiffies() except that it will never round down. This is useful for timeouts for which the exact time of firing does not matter too much, as long as they don’t fire too early.

unsigned long round_jiffies_up_relative(unsigned long j)
function to round jiffies up to a full second

Parameters

unsigned long j
the time in (relative) jiffies that should be rounded
Description

This is the same as round_jiffies_relative() except that it will never round down. This is useful for timeouts for which the exact time of firing does not matter too much, as long as they don’t fire too early.

void set_timer_slack(struct timer_list * timer, int slack_hz)
set the allowed slack for a timer

Parameters

struct timer_list * timer
the timer to be modified
int slack_hz
the amount of time (in jiffies) allowed for rounding
Description

Set the amount of time, in jiffies, that a certain timer has in terms of slack. By setting this value, the timer subsystem will schedule the actual timer somewhere between the time mod_timer() asks for, and that time plus the slack.

By setting the slack to -1, a percentage of the delay is used instead.

void init_timer_key(struct timer_list * timer, unsigned int flags, const char * name, struct lock_class_key * key)
initialize a timer

Parameters

struct timer_list * timer
the timer to be initialized
unsigned int flags
timer flags
const char * name
name of the timer
struct lock_class_key * key
lockdep class key of the fake lock used for tracking timer sync lock dependencies
Description

init_timer_key() must be done to a timer prior calling any of the other timer functions.

int mod_timer_pending(struct timer_list * timer, unsigned long expires)
modify a pending timer’s timeout

Parameters

struct timer_list * timer
the pending timer to be modified
unsigned long expires
new timeout in jiffies
Description

mod_timer_pending() is the same for pending timers as mod_timer(), but will not re-activate and modify already deleted timers.

It is useful for unserialized use of timers.

int mod_timer(struct timer_list * timer, unsigned long expires)
modify a timer’s timeout

Parameters

struct timer_list * timer
the timer to be modified
unsigned long expires
new timeout in jiffies
Description

mod_timer() is a more efficient way to update the expire field of an active timer (if the timer is inactive it will be activated)

mod_timer(timer, expires) is equivalent to:

del_timer(timer); timer->expires = expires; add_timer(timer);
Note that if there are multiple unserialized concurrent users of the same timer, then mod_timer() is the only safe way to modify the timeout, since add_timer() cannot modify an already running timer.

The function returns whether it has modified a pending timer or not. (ie. mod_timer() of an inactive timer returns 0, mod_timer() of an active timer returns 1.)

int mod_timer_pinned(struct timer_list * timer, unsigned long expires)
modify a timer’s timeout

Parameters

struct timer_list * timer
the timer to be modified
unsigned long expires
new timeout in jiffies
Description

mod_timer_pinned() is a way to update the expire field of an active timer (if the timer is inactive it will be activated) and to ensure that the timer is scheduled on the current CPU.

Note that this does not prevent the timer from being migrated when the current CPU goes offline. If this is a problem for you, use CPU-hotplug notifiers to handle it correctly, for example, cancelling the timer when the corresponding CPU goes offline.

mod_timer_pinned(timer, expires) is equivalent to:

del_timer(timer); timer->expires = expires; add_timer(timer);
void add_timer(struct timer_list * timer)
start a timer

Parameters

struct timer_list * timer
the timer to be added
Description

The kernel will do a ->function(->data) callback from the timer interrupt at the ->expires point in the future. The current time is ‘jiffies’.

The timer’s ->expires, ->function (and if the handler uses it, ->data) fields must be set prior calling this function.

Timers with an ->expires field in the past will be executed in the next timer tick.

void add_timer_on(struct timer_list * timer, int cpu)
start a timer on a particular CPU

Parameters

struct timer_list * timer
the timer to be added
int cpu
the CPU to start it on
Description

This is not very scalable on SMP. Double adds are not possible.

int del_timer(struct timer_list * timer)
deactive a timer.

Parameters

struct timer_list * timer
the timer to be deactivated
Description

del_timer() deactivates a timer - this works on both active and inactive timers.

The function returns whether it has deactivated a pending timer or not. (ie. del_timer() of an inactive timer returns 0, del_timer() of an active timer returns 1.)

int try_to_del_timer_sync(struct timer_list * timer)
Try to deactivate a timer

Parameters

struct timer_list * timer
timer do del
Description

This function tries to deactivate a timer. Upon successful (ret >= 0) exit the timer is not queued and the handler is not running on any CPU.

int del_timer_sync(struct timer_list * timer)
deactivate a timer and wait for the handler to finish.

Parameters

struct timer_list * timer
the timer to be deactivated
Description

This function only differs from del_timer() on SMP: besides deactivating the timer it also makes sure the handler has finished executing on other CPUs.

Synchronization rules: Callers must prevent restarting of the timer, otherwise this function is meaningless. It must not be called from interrupt contexts unless the timer is an irqsafe one. The caller must not hold locks which would prevent completion of the timer’s handler. The timer’s handler must not call add_timer_on(). Upon exit the timer is not queued and the handler is not running on any CPU.

Note

For !irqsafe timers, you must not hold locks that are held in
interrupt context while calling this function. Even if the lock has nothing to do with the timer in question. Here’s why:

CPU0 CPU1 —- —-

<SOFTIRQ> call_timer_fn();

base->running_timer = mytimer;
spin_lock_irq(somelock);
<IRQ>
spin_lock(somelock);
del_timer_sync(mytimer);
while (base->running_timer == mytimer);
Now del_timer_sync() will never return and never release somelock. The interrupt on the other CPU is waiting to grab somelock but it has interrupted the softirq that CPU0 is waiting to finish.

The function returns whether it has deactivated a pending timer or not.

signed long __sched schedule_timeout(signed long timeout)
sleep until timeout

Parameters

signed long timeout
timeout value in jiffies
Description

Make the current task sleep until timeout jiffies have elapsed. The routine will return immediately unless the current task state has been set (see set_current_state()).

You can set the task state as follows -

TASK_UNINTERRUPTIBLE - at least timeout jiffies are guaranteed to pass before the routine returns. The routine will return 0

TASK_INTERRUPTIBLE - the routine may return early if a signal is delivered to the current task. In this case the remaining time in jiffies will be returned, or 0 if the timer expired in time

The current task state is guaranteed to be TASK_RUNNING when this routine returns.

Specifying a timeout value of MAX_SCHEDULE_TIMEOUT will schedule the CPU away without a bound on the timeout. In this case the return value will be MAX_SCHEDULE_TIMEOUT.

In all cases the return value is guaranteed to be non-negative.

void msleep(unsigned int msecs)
sleep safely even with waitqueue interruptions

Parameters

unsigned int msecs
Time in milliseconds to sleep for
unsigned long msleep_interruptible(unsigned int msecs)
sleep waiting for signals

Parameters

unsigned int msecs
Time in milliseconds to sleep for

```c
void __sched usleep_range(unsigned long min, unsigned long max)
```
用于非原子环境的睡眠，目前内核建议用这个函数替换之前udelay。
这个函数最早会在形参min时间醒来，最晚会在max这个时间醒来。这个函数不是让cpu忙等待，而是让出cpu让其他进程使用，因此使用这个函数比udelay的效率高，因此推荐使用.

**参数**

`unsigned long min`
最小的微秒睡眠时间。

`unsigned long max`
最大的微秒睡眠时间。

### 4.2 Device drivers infrastructure

**按照drivers/目录下的顺序来**

#### 4.2.79 of

/**
 * of_get_named_gpio() - Get a GPIO number to use with GPIO API
 * @np:		device node to get GPIO from
 * @propname:	
 * @index:	index of the GPIO
 *
 * 
 */

```c
static inline int of_get_named_gpio(struct device_node *np,
                                    const char *propname, int index)
```

获取GPIO的数值.

**参数**

`struct device_node *np`
用来获取GPIO的设备节点。
`const char *propname`
包含gpio说明符的属性名称。
`int index`
gpio序号

**返回值**

返回与Linux通用GPIO API一起使用的GPIO编号，或者返回错误情况下的errno值之一。

**示例**

```c
keydev.nd = of_find_node_by_path("/key");
if (keydev.nd== NULL) {
    return -EINVAL;
}

keydev.key_gpio = of_get_named_gpio(keydev.nd ,"key-gpio", 0);
if (keydev.key_gpio < 0) {
    printk("can't get key0\r\n");
    return -EINVAL;
}
```

## 1.驱动框架

### 1.1 字符设备

==**内核：4.19.81**==

```c
int register_chrdev_region(dev_t from, unsigned count, 
                           const char * name)
```
注册一系列设备号

**参数**

`dev_t from`
所需设备编号范围中的第一个； 必须包含主号码。
`unsigned count`
所需的连续设备号的数量
`const char * name`
设备或驱动程序的名称。

**Description**

成功时返回值为零，失败时返回负错误代码。

示例:

```c
#define GPIOLED_CNT 1            /* 设备号个数 */
#define GPIOLED_NAME "gpioled"   /* 名字 */

gpioled.devid = MKDEV(gpioled.major, 0);
register_chrdev_region(gpioled.devid, GPIOLED_CNT,GPIOLED_NAME);
```

---

```c
int alloc_chrdev_region(dev_t * dev, unsigned baseminor,
                        unsigned count, const char * name)
```

register a range of char device numbers

Parameters

dev_t * dev
output parameter for first assigned number
unsigned baseminor
first of the requested range of minor numbers
unsigned count
the number of minor numbers required
const char * name
the name of the associated device or driver
Description

Allocates a range of char device numbers. The major number will be chosen dynamically, and returned (along with the first minor number) in dev. Returns zero or a negative error code.

int __register_chrdev(unsigned int major, unsigned int baseminor, unsigned int count, const char * name, const struct file_operations * fops)
create and register a cdev occupying a range of minors

Parameters

unsigned int major
major device number or 0 for dynamic allocation
unsigned int baseminor
first of the requested range of minor numbers
unsigned int count
the number of minor numbers required
const char * name
name of this range of devices
const struct file_operations * fops
file operations associated with this devices
Description

If major == 0 this functions will dynamically allocate a major and return its number.

If major > 0 this function will attempt to reserve a device with the given major number and will return zero on success.

Returns a -ve errno on failure.

The name of this device has nothing to do with the name of the device in /dev. It only helps to keep track of the different owners of devices. If your module name has only one type of devices it’s ok to use e.g. the name of the module here.

void unregister_chrdev_region(dev_t from, unsigned count)
unregister a range of device numbers

Parameters

dev_t from
the first in the range of numbers to unregister
unsigned count
the number of device numbers to unregister
Description

This function will unregister a range of count device numbers, starting with from. The caller should normally be the one who allocated those numbers in the first place...

void __unregister_chrdev(unsigned int major, unsigned int baseminor, unsigned int count, const char * name)
unregister and destroy a cdev

Parameters

unsigned int major
major device number
unsigned int baseminor
first of the range of minor numbers
unsigned int count
the number of minor numbers this cdev is occupying
const char * name
name of this range of devices
Description

Unregister and destroy the cdev occupying the region described by major, baseminor and count. This function undoes what __register_chrdev() did.

int cdev_add(struct cdev * p, dev_t dev, unsigned count)
add a char device to the system

Parameters

struct cdev * p
the cdev structure for the device
dev_t dev
the first device number for which this device is responsible
unsigned count
the number of consecutive minor numbers corresponding to this device
Description

cdev_add() adds the device represented by p to the system, making it live immediately. A negative error code is returned on failure.

void cdev_del(struct cdev * p)
remove a cdev from the system

Parameters

struct cdev * p
the cdev structure to be removed
Description

cdev_del() removes p from the system, possibly freeing the structure itself.

struct cdev * cdev_alloc(void)
allocate a cdev structure

Parameters

void
no arguments
Description

Allocates and returns a cdev structure, or NULL on failure.

void cdev_init(struct cdev * cdev, const struct file_operations * fops)
initialize a cdev structure

Parameters

struct cdev * cdev
the structure to initialize
const struct file_operations * fops
the file_operations for this device
Description

Initializes cdev, remembering fops, making it ready to add to the system with cdev_add().

/*
 * watchpmod.c - Linux kernel module for hardware watchpoints
 *
 * Copyright (C) 2025 Hunko Volodymyr
 *
 * Licensed under the GPL v2 only.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <linux/mutex.h>
#include <linux/param.h>
#include <linux/printk.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("enderavour");
MODULE_DESCRIPTION("Hardware watchpoint module - read/write handlers, backtrace");

// module parameter 
static unsigned long addr = 0;

MODULE_PARM_DESC(addr, "Address to watch (hex or decimal). Write to /sys/module/mywatchpoint/parameters/addr to change");

// perf_event pointers for read and write watchpoints
static struct perf_event *__percpu *bp_read;
static struct perf_event *__percpu *bp_write;
static DEFINE_MUTEX(bp_lock);

static void bp_read_handler(struct perf_event *bp, 
	struct perf_sample_data *data, struct pt_regs *regs
)
{
	pr_info("watchpmod: READ detected at addr=0x%lx", addr);
	dump_stack();
}

static void bp_write_handler(struct perf_event *bp, 
	struct perf_sample_data *data, struct pt_regs *regs
)
{
	pr_info("watchpmod: WRITE detected at addr=0x%lx", addr);
	dump_stack();
}

static int setup_breakpoints(unsigned long new_addr)
{
	struct perf_event_attr attr;
	int ret = 0;

	mutex_lock(&bp_lock);

	// unregister initial breakpoints
	if (bp_read)
	{
		unregister_wide_hw_breakpoint(bp_read);
		bp_read = NULL;
	}
	if (bp_write)
	{
		unregister_wide_hw_breakpoint(bp_write);
		bp_read = NULL;
	}

	if (new_addr == 0)
	{
		mutex_unlock(&bp_lock);
		pr_info("watchpmod: monitoring disabled (addr=0)");
		return 0;
	}

	hw_breakpoint_init(&attr);
	attr.bp_addr = new_addr;
	// Choose 1 by default; adjust it if want different width
	attr.bp_len = HW_BREAKPOINT_LEN_1;

	// Registering breakpoints
	attr.bp_type = HW_BREAKPOINT_R;
	bp_read = register_wide_hw_breakpoint(&attr, bp_read_handler, NULL);
	if (IS_ERR(bp_read))
	{
		pr_err("watchpmod: register write breakpoint failed: %ld\n", PTR_ERR(bp_read));
		bp_read = NULL;
		ret = PTR_ERR(bp_read);
		goto out;
	}

	attr.bp_type = HW_BREAKPOINT_W;
	bp_write = register_wide_hw_breakpoint(&attr, bp_write_handler, NULL);
	if (IS_ERR(bp_write))
	{
		pr_err("watchpmod: register write breakpoint failed: %ld\n", PTR_ERR(bp_write));
		unregister_wide_hw_breakpoint(bp_read);
		bp_read = NULL;
		bp_write = NULL;
		ret = PTR_ERR(bp_write);
		goto out;
	}

	pr_info("watchpointmod: monitoring 0x%lx (read+write)\n", new_addr);

out:
	mutex_unlock(&bp_lock);
	return ret;
}

// react immediately when written in param
static int addr_set(const char *val, const struct kernel_param *kp)
{
	unsigned long v;
	int ret;

	ret = kstrtoul(val, 0, &v);
	if (ret)
		return ret;

	ret = setup_breakpoints(v);

	// store value
	*((unsigned long *)kp->arg) = v;
	return 0;
}

static int addr_get(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "0x%lx\n", *((unsigned long*)kp->arg));
}

static const struct kernel_param_ops addr_ops = {
	.set = addr_set,
	.get = addr_get
};

module_param_cb(addr, &addr_ops, &addr, 0644);

static int __init watchpmod_init(void)
{
	int ret = 0;
	pr_info("watchpmod: init\n");

	if (addr != 0)
	{
		ret = setup_breakpoints(addr);
		if (ret)
		{
			pr_err("watchpmod: failed to setup initial breakpoints (%d)\n", ret);
			// continue module load but without monitoring
		}
	}
	return 0;
}

static void __exit watchpmod_exit(void)
{
	mutex_lock(&bp_lock);
	if (bp_read)
	{
		unregister_wide_hw_breakpoint(bp_read);
		bp_read = NULL;
	}

	if (bp_write)
	{
		unregister_wide_hw_breakpoint(bp_write);
		bp_write = NULL;
	}
	mutex_unlock(&bp_lock);

	pr_info("watchpmod: exit\n");
}

module_init(watchpmod_init);
module_exit(watchpmod_exit);

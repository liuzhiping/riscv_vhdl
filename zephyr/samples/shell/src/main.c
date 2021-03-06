/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <misc/shell.h>
#include <soc.h>
#define DEVICE_NAME "test shell"

extern void benchmark_dhrystone(int argc, char *argv[]);

static void shell_cmd_ping(int argc, char *argv[])
{
	printk("pong\n");
}

static void shell_cmd_ticks(int argc, char *argv[])
{
	printk("ticks: %d\n", sys_tick_get_32());
}

static void shell_cmd_highticks(int argc, char *argv[])
{
	printk("highticks: %x\n", (unsigned int)sys_cycle_get_64());
}

/**
 * Output SOC plug'n'Play information with detected devices
 */
static void shell_cmd_pnp(int argc, char *argv[])
{
#ifdef CONFIG_RISCV64
    soc_print_pnp();
#else
    printk("pnp not available\n");
#endif
}


const struct shell_cmd commands[] = {
	{ "ping", shell_cmd_ping },
	{ "ticks", shell_cmd_ticks },
	{ "highticks", shell_cmd_highticks },
    { "pnp", shell_cmd_pnp },
    { "dhry", benchmark_dhrystone},
	{ NULL, NULL }
};

void main(void)
{
	uint32_t version = sys_kernel_version_get();

	printk("Zephyr version %d.%d.%d\n",
		SYS_KERNEL_VER_MAJOR(version),
		SYS_KERNEL_VER_MINOR(version),
		SYS_KERNEL_VER_PATCHLEVEL(version));
	shell_init("shell> ", commands);
}

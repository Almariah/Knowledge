#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <asm/msr.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/topology.h>
#include <linux/slab.h> // For kmalloc and kfree

#define TIMER_INTERVAL 1 // Timer interval in seconds
#define PROC_FILE_NAME "cpu_frequencies"

static struct timer_list my_timer;
static char cpu_freq_data[4096]; // Increased buffer size to accommodate additional information
static struct proc_dir_entry *proc_file;

struct cpu_freq_info {
    unsigned int cur_freq; // Renamed from 'current' to 'cur_freq' to avoid conflict
    unsigned int max;
    unsigned int min;
};

static struct cpu_freq_info *cpu_freqs; // Dynamically allocated array to store frequency information for each CPU

static void read_cpu_frequencies(void) {
    unsigned int cpu;
    unsigned int *first_thread_per_core;

    // Dynamically allocate memory for first_thread_per_core array
    first_thread_per_core = kmalloc_array(NR_CPUS, sizeof(unsigned int), GFP_KERNEL);
    if (!first_thread_per_core) {
        pr_err("Failed to allocate memory for first_thread_per_core\n");
        return;
    }

    // Initialize the first_thread_per_core array
    for (cpu = 0; cpu < NR_CPUS; cpu++) {
        first_thread_per_core[cpu] = NR_CPUS;
    }

    for_each_online_cpu(cpu) {
        uint64_t msr_value;
        unsigned int freq;
        unsigned int core_id = topology_core_id(cpu);

        // If this core has not been seen before, mark this CPU as the first thread
        if (first_thread_per_core[core_id] == NR_CPUS) {
            first_thread_per_core[core_id] = cpu;
        }

        // Only read frequency if this is the first thread in the core
        if (cpu != first_thread_per_core[core_id]) {
            continue;
        }

        // Read the MSR value for the given CPU
        rdmsrl_on_cpu(cpu, MSR_IA32_PERF_STATUS, &msr_value);

        // Extract the frequency from the MSR value (simplified)
        freq = (msr_value >> 8) & 0xff;

        // Multiply by 100 MHz to get the frequency in MHz
        freq *= 100;

        // Update the current frequency
        cpu_freqs[cpu].cur_freq = freq;

        // Update the max and min frequencies
        if (freq > cpu_freqs[cpu].max) {
            cpu_freqs[cpu].max = freq;
        }
        if (freq < cpu_freqs[cpu].min || cpu_freqs[cpu].min == 0) {
            cpu_freqs[cpu].min = freq;
        }
    }

    // Free the dynamically allocated memory
    kfree(first_thread_per_core);
}

static void update_cpu_freq_data(void) {
    unsigned int cpu;
    char *ptr = cpu_freq_data;
    int len;
    unsigned int *first_thread_per_core;

    // Dynamically allocate memory for first_thread_per_core array
    first_thread_per_core = kmalloc_array(NR_CPUS, sizeof(unsigned int), GFP_KERNEL);
    if (!first_thread_per_core) {
        pr_err("Failed to allocate memory for first_thread_per_core\n");
        return;
    }

    // Initialize the first_thread_per_core array
    for (cpu = 0; cpu < NR_CPUS; cpu++) {
        first_thread_per_core[cpu] = NR_CPUS;
    }

    for_each_online_cpu(cpu) {
        unsigned int core_id = topology_core_id(cpu);

        // If this core has not been seen before, mark this CPU as the first thread
        if (first_thread_per_core[core_id] == NR_CPUS) {
            first_thread_per_core[core_id] = cpu;
        }

        // Only include frequency data if this is the first thread in the core
        if (cpu != first_thread_per_core[core_id]) {
            continue;
        }

        len = sprintf(ptr, "CPU %u Frequency: %u MHz, Max: %u MHz, Min: %u MHz\n",
                      cpu, cpu_freqs[cpu].cur_freq, cpu_freqs[cpu].max, cpu_freqs[cpu].min);
        ptr += len;
    }

    // Free the dynamically allocated memory
    kfree(first_thread_per_core);
}

// Timer callback function
static void my_timer_callback(struct timer_list *t) {
    read_cpu_frequencies();
    update_cpu_freq_data();
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL * HZ);
}

// Proc file read function
static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    return simple_read_from_buffer(buf, count, ppos, cpu_freq_data, strlen(cpu_freq_data));
}

// Proc ops structure
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

// Module initialization function
static int __init my_module_init(void) {
    unsigned int cpu;

    pr_info("CPU Frequency Monitor Module Loaded\n");

    // Dynamically allocate memory for cpu_freqs array
    cpu_freqs = kmalloc_array(NR_CPUS, sizeof(struct cpu_freq_info), GFP_KERNEL);
    if (!cpu_freqs) {
        pr_err("Failed to allocate memory for cpu_freqs\n");
        return -ENOMEM;
    }

    // Initialize min frequencies to 0 (to be updated later)
    for_each_possible_cpu(cpu) {
        cpu_freqs[cpu].min = 0;
    }

    // Create the proc file
    proc_file = proc_create(PROC_FILE_NAME, 0, NULL, &proc_fops);
    if (!proc_file) {
        pr_err("Failed to create /proc/%s\n", PROC_FILE_NAME);
        kfree(cpu_freqs);
        return -ENOMEM;
    }

    // Initialize and start the timer
    timer_setup(&my_timer, my_timer_callback, 0);
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL * HZ);

    // Read initial CPU frequencies
    read_cpu_frequencies();
    update_cpu_freq_data();

    return 0;
}

// Module exit function
static void __exit my_module_exit(void) {
    del_timer(&my_timer);
    proc_remove(proc_file);
    kfree(cpu_freqs); // Free the dynamically allocated memory
    pr_info("CPU Frequency Monitor Module Unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A kernel module to monitor CPU frequencies using MSRs and procfs");

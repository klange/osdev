/* vim: tabstop=4 shiftwidth=4 noexpandtab
 */

#ifndef PROCESS_H
#define PROCESS_H

//#include <system.h>
#include <tree.h>
#include <signal.h>
#include <task.h>

#define KERNEL_STACK_SIZE 0x8000

typedef signed int    pid_t;
typedef unsigned int  user_t;
typedef unsigned char status_t;

#define USER_ROOT_UID (user_t)0

/* Unix waitpid() options */
enum wait_option{
	WCONTINUED,
	WNOHANG,
	WUNTRACED
};

/* x86 task */
typedef struct thread {
	uintptr_t  esp; /* Stack Pointer */
	uintptr_t  ebp; /* Base Pointer */
	uintptr_t  eip; /* Instruction Pointer */

	uint8_t    fpu_enabled;
	uint8_t    fp_regs[512];

	uint8_t    padding[32]; /* I don't know */

	page_directory_t * page_directory; /* Page Directory */
} thread_t;

/* Portable image struct */
typedef struct image {
	size_t    size;        /* Image size */
	uintptr_t entry;       /* Binary entry point */
	uintptr_t heap;        /* Heap pointer */
	uintptr_t heap_actual; /* Actual heap location */
	uintptr_t stack;       /* Process kernel stack */
	uintptr_t user_stack;  /* User stack */
	uintptr_t start;
	uintptr_t shm_heap;
} image_t;

/* Resizable descriptor table */
typedef struct descriptor_table {
	fs_node_t ** entries;
	size_t       length;
	size_t       capacity;
	size_t       refs;
} fd_table_t;

/* XXX */
#define SIG_COUNT 10

/* Signal Table */
typedef struct signal_table {
	uintptr_t functions[NUMSIGNALS+1];
} sig_table_t;

/* Portable process struct */
typedef struct process {
	pid_t         id;                /* Process ID (pid) */
	char *        name;              /* Process Name */
	char *        description;       /* Process description */
	user_t        user;              /* Effective user */
	int           mask;              /* Umask */

	char **       cmdline;

	pid_t         group;             /* Process thread group */
	pid_t         job;               /* Process job group */
	pid_t         session;           /* Session group */

	thread_t      thread;            /* Associated task information */
	tree_node_t * tree_entry;        /* Process Tree Entry */
	image_t       image;             /* Binary image information */
	fs_node_t *   wd_node;           /* Working directory VFS node */
	char *        wd_name;           /* Working directory path name */
	fd_table_t *  fds;               /* File descriptor table */
	status_t      status;            /* Process status */
	sig_table_t   signals;           /* Signal table */
	uint8_t       finished;          /* Status indicator */
	uint8_t       started;
	struct regs * syscall_registers; /* Registers at interrupt */
	list_t *      wait_queue;
	list_t *      shm_mappings;      /* Shared memory chunk mappings */
	list_t *      signal_queue;      /* Queued signals */
	thread_t      signal_state;
	char *        signal_kstack;
	node_t        sched_node;
	node_t        sleep_node;

	int 			sending_or_receiving;        /* Is the process sending or receiving a message */
	int 			send_to;
	int 			recv_from;                   /* Send to/receive from which process */
	list_t *		sender_queue;      /* Sender queue */
	list_t *		receiving_queue;
	message_t *		msg;
} process_t;

#define PROC_RUNNING	0
#define PROC_SENDING	1
#define PROC_RECEIVING	2

#define PROC_ANY		-1
#define PROC_NO_TASK	-2

typedef struct {
	unsigned long end_tick;
	unsigned long end_subtick;
	process_t * process;
} sleeper_t;

void initialize_process_tree(void);
process_t * spawn_process(volatile process_t * parent);
void debug_print_process_tree(void);
process_t * spawn_init(void);
void set_process_environment(process_t * proc, page_directory_t * directory);
void make_process_ready(process_t * proc);
void make_process_reapable(process_t * proc);
uint8_t process_available(void);
process_t * next_ready_process(void);
uint8_t should_reap(void);
process_t * next_reapable_process(void);
uint32_t process_append_fd(process_t * proc, fs_node_t * node);
process_t * process_from_pid(pid_t pid);
process_t * process_get_first_child(process_t * process);
void delete_process(process_t * proc);
uint32_t process_move_fd(process_t * proc, int src, int dest);
int process_is_ready(process_t * proc);
void set_reaped(process_t * proc);

void wakeup_sleepers(unsigned long seconds, unsigned long subseconds);
void sleep_until(process_t * process, unsigned long seconds, unsigned long subseconds);

volatile process_t * current_process;

list_t * process_list;

#endif

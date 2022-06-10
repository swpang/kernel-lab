#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define BUFSIZE 4096

typedef struct debugfs_blob_wrapper dbw;

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

char *treebuf;
dbw *treeblob;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
	int i;
	pid_t input_pid;
	char buffer[BUFSIZE];

	sscanf(user_buffer, "%u", &input_pid);

	// Find current task_struct using input_pid. Hint: pid_task
	curr = pid_task(find_vpid(input_pid), PIDTYPE_PID);
	if (!curr) {
		printk("Error: PID not found\n");
		return -1;
	}

	/* Initialize output buffer */
	for (i = 0; i < BUFSIZE; i++)
		treebuf[i] = 0;

	/* Tracing process tree from input_pid to init(1) process 
	 * - write each entry to a buffer (buffer = global variable)	
	 */
	while (curr) {
		/* Make Output Format string: process_command (process_id) */
		sprintf(buffer, "%s (%d)\n", curr->comm, curr->pid);
		strcat(buffer, treebuf);		/* concat to buffer, then copy back to treebuf */
		strcpy(treebuf, buffer);		/* in order to make sure the higher level parent goes in front of the lower level parent */
		printk("%s (%d)", curr->comm, curr->pid);
		if (curr->pid == 1)				/* iterate on parents until pid = 1 */
			break;
		else
			curr = curr->real_parent;	
	}

	return length;
}

static const struct file_operations dbfs_fops = {
    .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
	dir = debugfs_create_dir("ptree", NULL);
	
	if (!dir) {
		printk("Error: Cannot create ptree dir\n");
		return -1;
	}

	/* Initialize data structures needed */
	treebuf = (char *) kmalloc(BUFSIZE * sizeof(char), GFP_KERNEL);
	treeblob = (dbw *) kmalloc(sizeof(dbw), GFP_KERNEL);
	treeblob->data = (void *) treebuf;			/* Point the data pointer of the blob wrapper to the output buffer */
	treeblob->size = BUFSIZE * sizeof(char);	

	inputdir = debugfs_create_file("input", 0644, dir, NULL, &dbfs_fops);	/* User-writeable */
	ptreedir = debugfs_create_blob("ptree", 0444, dir, treeblob); 			/* Read-only */

	printk("dbfs_ptree module initialize done\n");

	return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);		/* Remove create directories */
	kfree(treeblob);					/* Free kernel memory allocated to data structures */
	kfree(treebuf);

	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);

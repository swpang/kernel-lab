#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/pagewalk.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

struct packet {
	pid_t pid;
	unsigned long vaddr;
	unsigned long paddr;
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
	unsigned long vaddr, ppn, ppo;
	pid_t pid;
	pgd_t *pgd;		/* Page Global Directory offset address */
	p4d_t *p4d;		/* Page 4th Level Directory offset address */
	pud_t *pud;		/* Page Upper Directory offset address */
	pmd_t *pmd;		/* Page Middle Directory offset address */
	pte_t *pte;		/* Page Table Entry offset address */

	/* Get pid and virtual address of target process from the user_buffer */
	struct packet *buffer = (struct packet *) user_buffer;
	pid = buffer->pid;
	vaddr = buffer->vaddr;

	task = pid_task(find_vpid(pid), PIDTYPE_PID);

	/* Page walking PGD->P4D->PUD->PMD->PT */
	pgd = pgd_offset(task->mm, vaddr);			/* Calculate PGD offset from current process */
	if (pgd_none(*pgd)) {
		printk("Error: not mapped in page global directory\n");
		return -1;
	}
	printk("PGD offset: %lu\n", pgd->pgd);

	p4d = p4d_offset(pgd, vaddr);				/* Calculate P4D offset from PGD offset */
	if (p4d_none(*p4d)) {
		printk("Error: not mapped in page 4th level directory\n");
		return -1;
	}
	printk("P4D offset: %lu\n", p4d->p4d);

	pud = pud_offset(p4d, vaddr);				/* Calculate PUD offset from P4D offset */
	if (pud_none(*pud)) {
		printk("Error: not mapped in page upper directory\n");
		return -1;
	}
	printk("PUD offset: %lu\n", pud->pud);

	pmd = pmd_offset(pud, vaddr);				/* Calculate PMD offset from PUD offset */
	if (pmd_none(*pmd)) {
		printk("Error: not mapped in page middle directory\n");
		return -1;
	}
	printk("PMD offset: %lu\n", pmd->pmd);

	pte = pte_offset_kernel(pmd, vaddr);		/* Calculate PTE offset from PMD offset */
	if (pte_none(*pte)) {
		printk("Error: not mapped in page table\n");
		return -1;
	}
	printk("PTE offset: %lu\n", pte->pte);

	ppn = pte_pfn(*pte);						/* Get Physical Page Number */
	ppo = vaddr & ~PAGE_MASK;					/* Get Physical Page Offset from Virtual Address */
	buffer->paddr = (ppn << PAGE_SHIFT) | ppo;	/* Combine page number and offset to form Physical Address */		

	printk("PPN: %ld\n", ppn);
	printk("PPO: %ld\n", ppo);
	printk("vaddr = %ld -> paddr = %ld\n", buffer->vaddr, buffer->paddr);
	
	return length;
}

static const struct file_operations dbfs_fops = {
	.read = read_output,
};

static int __init dbfs_module_init(void)
{
	dir = debugfs_create_dir("paddr", NULL);

	if (!dir) {
		printk("Error: Cannot create paddr dir\n");
		return -1;
	}

	output = debugfs_create_file("output", 0444, dir, NULL, &dbfs_fops);

	printk("dbfs_paddr module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir);			/* Remove create directories */
	printk("dbfs_paddr module exit\n");		/* Free kernel memory allocated to data structures */
}

module_init(dbfs_module_init);	
module_exit(dbfs_module_exit);	

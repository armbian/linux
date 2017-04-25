#include <linux/module.h>

MODULE_AUTHOR("Luis R. Rodriguez");
MODULE_DESCRIPTION("Kernel compatibility module");
MODULE_LICENSE("GPL");

#ifndef COMPAT_BASE_TREE
#error "You need a COMPAT_BASE_TREE"
#endif

#ifndef COMPAT_BASE_TREE_VERSION
#error "You need a COMPAT_BASE_TREE_VERSION"
#endif

#ifndef COMPAT_VERSION
#error "You need a COMPAT_VERSION"
#endif

static char *compat_base_tree = COMPAT_BASE_TREE;
static char *compat_base_tree_version = COMPAT_BASE_TREE_VERSION;
static char *compat_version = COMPAT_VERSION;

module_param(compat_base_tree, charp, 0400);
MODULE_PARM_DESC(compat_base_tree,
		 "The upstream tree used as base for this backport");

module_param(compat_base_tree_version, charp, 0400);
MODULE_PARM_DESC(compat_base_tree_version,
		 "The git-describe of the upstream base tree");

module_param(compat_version, charp, 0400);
MODULE_PARM_DESC(compat_version,
		 "Version of the kernel compat backport work");

static int __init compat_init(void)
{
	/* pm-qos for kernels <= 2.6.24, this is a no-op on newer kernels */
	compat_pm_qos_power_init();
	compat_system_workqueue_create();

	printk(KERN_INFO
	       COMPAT_PROJECT " backport release: "
	       COMPAT_VERSION
	       "\n");
	printk(KERN_INFO "Backport based on "
	       COMPAT_BASE_TREE " " COMPAT_BASE_TREE_VERSION
	       "\n");

	return 0;
}
module_init(compat_init);

static void __exit compat_exit(void)
{
	compat_pm_qos_power_deinit();
	compat_system_workqueue_destroy();

	return;
}
module_exit(compat_exit);


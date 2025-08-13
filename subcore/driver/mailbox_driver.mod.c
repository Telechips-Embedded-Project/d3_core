#include <linux/module.h>
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0xb3bd593a, "module_layout" },
	{ 0x3feb65b7, "param_ops_uint" },
	{ 0xe9eed733, "platform_driver_unregister" },
	{ 0xa2b73f10, "__platform_driver_register" },
	{ 0x6ed26257, "mbox_send_message" },
	{ 0xaf507de1, "__arch_copy_from_user" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x6b2941b2, "__arch_copy_to_user" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x281823c5, "__kfifo_out_peek" },
	{ 0x5e811e0e, "finish_wait" },
	{ 0xb8fb810a, "prepare_to_wait_event" },
	{ 0x1000e51, "schedule" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0xbac53caa, "_dev_info" },
	{ 0xd3be2641, "_dev_err" },
	{ 0xfb8d8361, "device_create" },
	{ 0x8843d28d, "cdev_add" },
	{ 0x6f9bd303, "cdev_init" },
	{ 0x8a0a8b6d, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x2f981aa9, "mbox_request_channel_byname" },
	{ 0x139f2189, "__kfifo_alloc" },
	{ 0x21621fc4, "__mutex_init" },
	{ 0xa2a635bc, "__init_waitqueue_head" },
	{ 0x1e81cb80, "of_property_read_string" },
	{ 0x72526d15, "devm_kmalloc" },
	{ 0x34274c5f, "_dev_warn" },
	{ 0x7f85bfb1, "mutex_unlock" },
	{ 0x4825e296, "__wake_up" },
	{ 0xf23fcb99, "__kfifo_in" },
	{ 0x65f63d13, "mutex_lock" },
	{ 0xdb760f52, "__kfifo_free" },
	{ 0xfd259a3b, "mbox_free_channel" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x1d94ab8d, "class_destroy" },
	{ 0x98f9a4e4, "cdev_del" },
	{ 0x23008c11, "device_destroy" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Ctelechips,mbox_test");
MODULE_ALIAS("of:N*T*Ctelechips,mbox_testC*");

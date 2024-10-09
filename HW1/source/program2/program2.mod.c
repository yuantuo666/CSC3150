#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
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
__used __section("__versions") = {
	{ 0x962cdbfe, "module_layout" },
	{ 0xe4de1687, "wake_up_process" },
	{ 0x4981fdc, "kthread_create_on_node" },
	{ 0x51486863, "kernel_wait" },
	{ 0x7a637f1e, "kernel_clone" },
	{ 0xdcb764ad, "memset" },
	{ 0x9f49dcc4, "__stack_chk_fail" },
	{ 0x576567f8, "putname" },
	{ 0x9d1e9489, "kernel_execve" },
	{ 0x85416d23, "getname_kernel" },
	{ 0xf9a482f9, "msleep" },
	{ 0xc5850110, "printk" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "BFFB17F0D67CEE2721D9368");

#include <linux/kernel.h>/*内核头文件，含有内核一些常用函数的原型定义*/
#include <linux/slab.h>/*定义内存分配的一些函数*/
#include <linux/module.h>/*模块编译必须的头文件*/
#include <linux/input.h>/*输入设备相关函数的头文件*/
#include <linux/init.h>/*linux初始化模块函数定义*/
#include <linux/usb.h> /*USB设备相关函数定义*/

static unsigned char usb_kbd_keycode[256] = {
0, 0, 0, 0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 2, 3,
4, 5, 6, 7, 8, 9, 10, 11, 28, 1, 14, 15, 57, 12, 13, 26,
27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
115,114, 0, 0, 0,121, 0, 89, 93,124, 92, 94, 95, 0, 0, 0,
122,123, 90, 91, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
150,158,159,128,136,177,178,176,142,152,173,140
};

static struct usb_device_id usb_kbd_id_table [] = {
	{USB_INTERFACE_INFO(3, 1, 1)},/*3,1,1分别表示接口类,接口子类,接口协议;3,1,1为键盘接口类;鼠标为3,1,2*/
	{} /* Terminating entry */
};//必须与0结束

MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);/*指定设备ID表*/

struct usb_kbd {
	struct input_dev *dev; /*定义一个输入设备*/
	struct usb_device *usbdev;/*定义一个usb设备*/
	unsigned char old[8]; /*按键离开时所用之数据缓冲区*/
	struct urb *irq/*usb键盘之中断请求块*/, *led/*usb键盘之指示灯请求块*/;
	unsigned char newleds;/*目标指定灯状态*/
	char name[128];/*存放厂商名字及产品名字*/
	char phys[64];/*设备之节点*/
	unsigned char *new;/*按键按下时所用之数据缓冲区*/
	struct usb_ctrlrequest *cr;/*控制请求结构*/
	unsigned char *leds;/*当前指示灯状态*/
	dma_addr_t cr_dma; /*控制请求DMA缓冲地址*/
	dma_addr_t new_dma; /*中断urb会使用该DMA缓冲区*/
	dma_addr_t leds_dma; /*指示灯DAM缓冲地址*/
};

/*USB键盘驱动结构体*/
static struct usb_driver usb_kbd_driver = {
	.name = "usbkbd",/*驱动名字*/
	.probe = usb_kbd_probe,/*驱动探测函数,加载时用到*/
	.disconnect = usb_kbd_disconnect,/*驱动断开函数,在卸载时用到，可选*/
	.id_table = usb_kbd_id_table,/*驱动设备ID表,用来指定设备或接口*/
};

/*驱动程序生命周期的开始点，向 USB core 注册这个键盘驱动程序。*/
static int __init usb_kbd_init(void)
{
	int result = usb_register(&usb_kbd_driver);/*注册USB键盘驱动*/
	if (result == 0) /*注册失败*/
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return result;
}

/* 驱动程序生命周期的结束点，向 USB core 注销这个键盘驱动程序。 */
static void __exit usb_kbd_exit(void)
{
	printk("SUNWILL-USBKBD:usb_kbd_exit begin...\n");
	usb_deregister(&usb_kbd_driver);/*注销USB键盘驱动*/
}

module_init(usb_kbd_init);
module_exit(usb_kbd_exit);

static void usb_kbd_irq(struct urb *urb, struct pt_regs *regs) {
	struct usb_kbd *kbd = urb->context;
	int i;
	switch (urb->status) {
	case 0: /* success */
		break;
	case -ECONNRESET: /* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	/* -EPIPE: should clear the halt */
	default: /* error */
		goto resubmit;
	}

	//input_regs(kbd->dev, regs);
	/*不知道其用意, 注释掉该部分仍可正常工作*/
	for (i = 0; i < 8; i++)/*8次的值依次是:29-42-56-125-97-54-100-126*/{
		input_report_key(kbd->dev, usb_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);
	}

	/*若同时只按下1个按键则在第[2]个字节,若同时有两个按键则第二个在第[3]字节，类推最多可有6个按键同时按下*/

	for (i = 2; i < 8; i++) {
	/*获取键盘离开的中断*/
		if (kbd->old > 3 && memscan(kbd->new + 2, kbd->old, 6) == kbd->new + 8) {/*同时没有该KEY的按下状态*/
			if (usb_kbd_keycode[kbd->old]) {
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->old], 0);
			} else
				info("Unknown key (scancode %#x) released.", kbd->old);
			}
		/*获取键盘按下的中断*/
		if (kbd->new > 3 && memscan(kbd->old + 2, kbd->new, 6) == kbd->old + 8) {/*同时没有该KEY的离开状态*/
			if (usb_kbd_keycode[kbd->new]) {
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->new], 1);
			} else
				info("Unknown key (scancode %#x) pressed.", kbd->new);
		}
	}
	/*同步设备,告知事件的接收者驱动已经发出了一个完整的报告*/
	input_sync(kbd->dev);
	memcpy(kbd->old, kbd->new, 8);/*防止未松开时被当成新的按键处理*/
resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC);/*发送USB请求块*/
	if (i)
	err ("can't resubmit intr, %s-%s/input0, status %d",
	kbd->usbdev->bus->bus_name,
	kbd->usbdev->devpath, i);
}

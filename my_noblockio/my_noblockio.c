/***************************************************************
Copyright ? ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
�ļ���		: my_noblockio.c
����	  	: ��ĳ��
�汾	   	: V1.0
����	   	: ������IO����
����	   	: ��
��־	   	: ����V1.0 2023/07/24 
***************************************************************/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEY_CNT			1		/* �豸�Ÿ��� 	*/
#define KEY_NAME		"key"	/* ���� 		*/

/* ���尴��״̬ */
enum key_status {
    KEY_PRESS = 0,      // ��������
    KEY_RELEASE,        // �����ɿ�
    KEY_KEEP,           // ����״̬����
};

/* key�豸�ṹ�� */
struct key_dev{
	dev_t devid;			/* �豸�� 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* �� 		*/
	struct device *device;	/* �豸 	 */
	struct device_node	*nd; /* �豸�ڵ� */
	int key_gpio;			/* key��ʹ�õ�GPIO���		*/
	struct timer_list timer;			/* ����ֵ 		*/
	int irq_num;			/* �жϺ� 		*/
	
	atomic_t status;   		/* ����״̬ */
	wait_queue_head_t r_wait;	/* ���ȴ�����ͷ */
};

static struct key_dev key;          /* �����豸 */

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	/* ������������������ʱ����ʱ15ms */
	mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));
    return IRQ_HANDLED;
}

//�豸������
static int key_parse_dt(void)
{
	int ret;
	const char *str;
	
	/* ����LED��ʹ�õ�GPIO */
	/* 1����ȡ�豸�ڵ㣺key */
	key.nd = of_find_node_by_path("/key");
	if(key.nd == NULL) {
		printk("key node not find!\r\n");
		return -EINVAL;
	}

	/* 2.��ȡstatus���� */
	ret = of_property_read_string(key.nd, "status", &str);
	if(ret < 0) 
	    return -EINVAL;

	if (strcmp(str, "okay"))
        return -EINVAL;
    
	/* 3����ȡcompatible����ֵ������ƥ�� */
	ret = of_property_read_string(key.nd, "compatible", &str);
	if(ret < 0) {
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	}

    if (strcmp(str, "alientek,key")) {
        printk("key: Compatible match failed\n");
        return -EINVAL;
    }

	/* 4�� ��ȡ�豸���е�gpio���ԣ��õ�KEY0��ʹ�õ�KYE��� */
	key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
	if(key.key_gpio < 0) {
		printk("can't get key-gpio");
		return -EINVAL;
	}

    /* 5 ����ȡGPIO��Ӧ���жϺ� */
    key.irq_num = irq_of_parse_and_map(key.nd, 0);
    if(!key.irq_num){
        return -EINVAL;
    }

	printk("key-gpio num = %d\r\n", key.key_gpio);
	return 0;
}

//�жϳ�ʼ��
static int key_gpio_init(void)
{
	int ret;
    unsigned long irq_flags;
	
	ret = gpio_request(key.key_gpio, "KEY0");
    if (ret) {
        printk(KERN_ERR "key: Failed to request key-gpio\n");
        return ret;
	}	
	
	/* ��GPIO����Ϊ����ģʽ */
    gpio_direction_input(key.key_gpio);

   /* ��ȡ�豸����ָ�����жϴ������� */
	irq_flags = irq_get_trigger_type(key.irq_num);
	if (IRQF_TRIGGER_NONE == irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
		
	/* �����ж� */
	ret = request_irq(key.irq_num, key_interrupt, irq_flags, "Key0_IRQ", NULL);
	if (ret) {
        gpio_free(key.key_gpio);
        return ret;
    }

	return 0;
}

static void key_timer_function(struct timer_list *arg)
{
    static int last_val = 1;
    int current_val;

    /* ��ȡ����ֵ���жϰ�����ǰ״̬ */
    current_val = gpio_get_value(key.key_gpio);
    if (0 == current_val && last_val){
        atomic_set(&key.status, KEY_PRESS);	// ����
		wake_up_interruptible(&key.r_wait);	// ����r_wait����ͷ�е����ж���
	}
    else if (1 == current_val && !last_val) {
        atomic_set(&key.status, KEY_RELEASE);   // �ɿ�
		wake_up_interruptible(&key.r_wait);	// ����r_wait����ͷ�е����ж���
	}
    else
        atomic_set(&key.status, KEY_KEEP);              // ״̬����

    last_val = current_val;
}

static int key_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf,
            size_t cnt, loff_t *offt)
{
	
	if (filp->f_flags & O_NONBLOCK) {	// ��������ʽ����
        if(KEY_KEEP == atomic_read(&key.status))
            return -EAGAIN;
    }else{
        printk("only noblock allowed");
    }
	return -1;
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

static int key_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

    poll_wait(filp, &key.r_wait, wait);

    if(KEY_KEEP != atomic_read(&key.status))	// �������»��ɿ���������
		mask = POLLIN | POLLRDNORM;	// ����PLLIN

    return mask;
}

/* �豸�������� */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = 	key_release,
	.poll = key_poll,
};

//������ں���
static int __init mykey_init(void)
{
	int ret;
	
	/* ��ʼ���ȴ�����ͷ */
	init_waitqueue_head(&key.r_wait);
	
	/* ��ʼ������״̬ */
	atomic_set(&key.status, KEY_KEEP);

	/* �豸������ */
	ret = key_parse_dt();
	if(ret)
		return ret;
		
	/* GPIO �жϳ�ʼ�� */
	ret = key_gpio_init();
	if(ret)
		return ret;
		
	/* ע���ַ��豸���� */
	/* 1�������豸�� */
	ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);	/* �����豸�� */
	if(ret < 0) {
		pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY_NAME, ret);
		goto free_gpio;
	}
	
	/* 2����ʼ��cdev */
	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);
	
	/* 3�����һ��cdev */
	ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4�������� */
	key.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(key.class)) {
		goto del_cdev;
	}

	/* 5�������豸 */
	key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
	if (IS_ERR(key.device)) {
		goto destroy_class;
	}
	
	/* 6����ʼ��timer�����ö�ʱ��������,��δ�������ڣ����в��ἤ�ʱ�� */
	timer_setup(&key.timer, key_timer_function, 0);
	
	return 0;

destroy_class:
	device_destroy(key.class, key.devid);
del_cdev:
	cdev_del(&key.cdev);
del_unregister:
	unregister_chrdev_region(key.devid, KEY_CNT);
free_gpio:
	free_irq(key.irq_num, NULL);
	gpio_free(key.key_gpio);
	return -EIO;
}

static void __exit mykey_exit(void)
{
	/* ע���ַ��豸���� */
	cdev_del(&key.cdev);/*  ɾ��cdev */
	unregister_chrdev_region(key.devid, KEY_CNT); /* ע���豸�� */
	del_timer_sync(&key.timer);		/* ɾ��timer */
	device_destroy(key.class, key.devid);/*ע���豸 */
	class_destroy(key.class); 		/* ע���� */
	free_irq(key.irq_num, NULL);	/* �ͷ��ж� */
	gpio_free(key.key_gpio);		/* �ͷ�IO */
}

module_init(mykey_init);
module_exit(mykey_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");


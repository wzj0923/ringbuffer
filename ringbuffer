#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#define DEVICE_NAME "ringbuf"
#define CLASS_NAME "ringbuf_class"
#define BUFFER_SIZE 4096  // 4KB环形缓冲区

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Ring Buffer Character Device Driver");

// 环形缓冲区结构体
struct ring_buffer {
    char *buffer;           // 缓冲区内存
    int head;               // 写指针（生产者）
    int tail;               // 读指针（消费者）
    int count;              // 当前数据量
    wait_queue_head_t read_queue;   // 读等待队列
    wait_queue_head_t write_queue;  // 写等待队列
    spinlock_t lock;        // 自旋锁保护并发访问
};

// 设备结构体
struct ringbuf_dev {
    struct cdev cdev;
    struct class *dev_class;
    struct device *dev_device;
    dev_t dev_num;
    struct ring_buffer *rb;
};

static struct ringbuf_dev *ringbuf_device;

// 初始化环形缓冲区
static struct ring_buffer *ring_buffer_init(void)
{
    struct ring_buffer *rb;
    
    rb = kmalloc(sizeof(struct ring_buffer), GFP_KERNEL);
    if (!rb)
        return NULL;
    
    rb->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!rb->buffer) {
        kfree(rb);
        return NULL;
    }
    
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    init_waitqueue_head(&rb->read_queue);
    init_waitqueue_head(&rb->write_queue);
    spin_lock_init(&rb->lock);
    
    return rb;
}

// 销毁环形缓冲区
static void ring_buffer_destroy(struct ring_buffer *rb)
{
    if (rb) {
        if (rb->buffer)
            kfree(rb->buffer);
        kfree(rb);
    }
}

// 向环形缓冲区写入数据（生产者）
static int ring_buffer_write(struct ring_buffer *rb, const char __user *user_data, size_t len)
{
    int i;
    size_t space;
    size_t bytes_written = 0;
    unsigned long flags;
    
    spin_lock_irqsave(&rb->lock, flags);
    
    // 检查缓冲区是否有空间
    if (rb->count == BUFFER_SIZE) {
        // 缓冲区满，返回0表示无法写入
        spin_unlock_irqrestore(&rb->lock, flags);
        return 0;
    }
    
    // 计算可用空间
    space = BUFFER_SIZE - rb->count;
    if (len > space)
        len = space;
    
    // 从用户空间拷贝数据到环形缓冲区
    for (i = 0; i < len; i++) {
        char ch;
        if (copy_from_user(&ch, user_data + i, 1)) {
            spin_unlock_irqrestore(&rb->lock, flags);
            return -EFAULT;
        }
        rb->buffer[rb->head] = ch;
        rb->head = (rb->head + 1) % BUFFER_SIZE;
        rb->count++;
        bytes_written++;
    }
    
    spin_unlock_irqrestore(&rb->lock, flags);
    
    // 唤醒等待读的进程
    wake_up_interruptible(&rb->read_queue);
    
    return bytes_written;
}

// 从环形缓冲区读取数据（消费者）
static ssize_t ring_buffer_read(struct ring_buffer *rb, char __user *user_data, size_t len)
{
    int i;
    size_t bytes_read = 0;
    unsigned long flags;
    
    spin_lock_irqsave(&rb->lock, flags);
    
    // 缓冲区空，返回0
    if (rb->count == 0) {
        spin_unlock_irqrestore(&rb->lock, flags);
        return 0;
    }
    
    // 计算可读取数据量
    if (len > rb->count)
        len = rb->count;
    
    // 拷贝数据到用户空间
    for (i = 0; i < len; i++) {
        char ch = rb->buffer[rb->tail];
        if (copy_to_user(user_data + i, &ch, 1)) {
            spin_unlock_irqrestore(&rb->lock, flags);
            return -EFAULT;
        }
        rb->tail = (rb->tail + 1) % BUFFER_SIZE;
        rb->count--;
        bytes_read++;
    }
    
    spin_unlock_irqrestore(&rb->lock, flags);
    
    // 唤醒等待写的进程
    wake_up_interruptible(&rb->write_queue);
    
    return bytes_read;
}

// 设备打开函数
static int ringbuf_open(struct inode *inode, struct file *filp)
{
    struct ringbuf_dev *dev = container_of(inode->i_cdev, struct ringbuf_dev, cdev);
    filp->private_data = dev;
    printk(KERN_INFO "ringbuf: device opened\n");
    return 0;
}

// 设备释放函数
static int ringbuf_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "ringbuf: device closed\n");
    return 0;
}

// 读函数
static ssize_t ringbuf_read_func(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct ringbuf_dev *dev = filp->private_data;
    ssize_t ret;
    
    ret = ring_buffer_read(dev->rb, buf, count);
    return ret;
}

// 写函数
static ssize_t ringbuf_write_func(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct ringbuf_dev *dev = filp->private_data;
    ssize_t ret;
    
    ret = ring_buffer_write(dev->rb, buf, count);
    return ret;
}

// 文件操作结构体
static struct file_operations ringbuf_fops = {
    .owner = THIS_MODULE,
    .open = ringbuf_open,
    .release = ringbuf_release,
    .read = ringbuf_read_func,
    .write = ringbuf_write_func,
};

// 模块初始化函数
static int __init ringbuf_init(void)
{
    int ret;
    
    printk(KERN_INFO "ringbuf: initializing driver\n");
    
    // 分配设备结构体内存
    ringbuf_device = kmalloc(sizeof(struct ringbuf_dev), GFP_KERNEL);
    if (!ringbuf_device) {
        printk(KERN_ALERT "ringbuf: failed to allocate device structure\n");
        return -ENOMEM;
    }
    memset(ringbuf_device, 0, sizeof(struct ringbuf_dev));
    
    // 分配设备号（动态分配）
    ret = alloc_chrdev_region(&ringbuf_device->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "ringbuf: failed to allocate device number\n");
        kfree(ringbuf_device);
        return ret;
    }
    printk(KERN_INFO "ringbuf: allocated major=%d, minor=%d\n", 
           MAJOR(ringbuf_device->dev_num), MINOR(ringbuf_device->dev_num));
    
    // 初始化cdev
    cdev_init(&ringbuf_device->cdev, &ringbuf_fops);
    ringbuf_device->cdev.owner = THIS_MODULE;
    ret = cdev_add(&ringbuf_device->cdev, ringbuf_device->dev_num, 1);
    if (ret < 0) {
        printk(KERN_ALERT "ringbuf: failed to add cdev\n");
        unregister_chrdev_region(ringbuf_device->dev_num, 1);
        kfree(ringbuf_device);
        return ret;
    }
    
    ringbuf_device->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(ringbuf_device->dev_class)) {
        printk(KERN_ALERT "ringbuf: failed to create class\n");
        cdev_del(&ringbuf_device->cdev);
        unregister_chrdev_region(ringbuf_device->dev_num, 1);
        kfree(ringbuf_device);
        return PTR_ERR(ringbuf_device->dev_class);
    }
    
    // 创建设备节点
    ringbuf_device->dev_device = device_create(ringbuf_device->dev_class, NULL,
                                                ringbuf_device->dev_num, NULL,
                                                DEVICE_NAME);
    if (IS_ERR(ringbuf_device->dev_device)) {
        printk(KERN_ALERT "ringbuf: failed to create device\n");
        class_destroy(ringbuf_device->dev_class);
        cdev_del(&ringbuf_device->cdev);
        unregister_chrdev_region(ringbuf_device->dev_num, 1);
        kfree(ringbuf_device);
        return PTR_ERR(ringbuf_device->dev_device);
    }
    
    // 初始化环形缓冲区
    ringbuf_device->rb = ring_buffer_init();
    if (!ringbuf_device->rb) {
        printk(KERN_ALERT "ringbuf: failed to init ring buffer\n");
        device_destroy(ringbuf_device->dev_class, ringbuf_device->dev_num);
        class_destroy(ringbuf_device->dev_class);
        cdev_del(&ringbuf_device->cdev);
        unregister_chrdev_region(ringbuf_device->dev_num, 1);
        kfree(ringbuf_device);
        return -ENOMEM;
    }
    
    printk(KERN_INFO "ringbuf: driver initialized successfully\n");
    return 0;
}

// 模块退出函数
static void __exit ringbuf_exit(void)
{
    printk(KERN_INFO "ringbuf: exiting driver\n");
    
    // 销毁环形缓冲区
    if (ringbuf_device->rb)
        ring_buffer_destroy(ringbuf_device->rb);
    
    // 销毁设备节点
    device_destroy(ringbuf_device->dev_class, ringbuf_device->dev_num);
    class_destroy(ringbuf_device->dev_class);
    
    // 删除cdev
    cdev_del(&ringbuf_device->cdev);
    
    // 释放设备号
    unregister_chrdev_region(ringbuf_device->dev_num, 1);
    
    // 释放设备结构体
    kfree(ringbuf_device);
    
    printk(KERN_INFO "ringbuf: driver exited\n");
}

module_init(ringbuf_init);
module_exit(ringbuf_exit);

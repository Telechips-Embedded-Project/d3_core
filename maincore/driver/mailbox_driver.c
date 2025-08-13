// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Telechips TCC805x Multi-channel Mailbox - Simple RW Client
 * - Binds to DT nodes with compatible "telechips,mbox_test"
 * - Exposes /dev/<device-name>
 * - write(): send struct tcc_mbox_data
 * - read()/poll(): receive struct tcc_mbox_data (blocking)
 *
 * DT example:
 *   mbox_test0: mbox_test0@0 {
 *       compatible = "telechips,mbox_test";
 *       device-name = "cb_mbox_test";
 *       mboxes = <&a72_sc_mbox 0x01000000>; // or other provider+channel
 *       mbox-names = "tx";
 *       status = "okay";
 *       // 선택: 클라이언트 노드에도 wakeup-source를 달 수 있으나,
 *       // 실제 웨이크업은 컨트롤러 IRQ가 관건이므로 컨트롤러 노드에 달아야 효과적임.
 *       // wakeup-source;
 *   };
 *   mbox_test1: mbox_test1@1 {
 *       compatible = "telechips,mbox_test";
 *       device-name = "micom_mbox_test";
 *       mboxes = <&a72_sc_mbox 0x02000000>;
 *       mbox-names = "tx";
 *       status = "okay";
 *       // wakeup-source;
 *   };
 *
 *   // 중요: 메일박스 컨트롤러 노드에 wakeup-source 추가
 *   // &a72_sc_mbox {
 *   //     status = "okay";
 *   //     wakeup-source;
 *   // };
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/mailbox_client.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/pm_wakeup.h>
#include <linux/pm.h>

#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox.h> /* struct tcc_mbox_data */

/* ----- Config ----- */
#ifndef MBOX_CMD_FIFO_SIZE
#define MBOX_CMD_FIFO_SIZE 6
#endif
#ifndef MBOX_DATA_FIFO_SIZE
#define MBOX_DATA_FIFO_SIZE 64
#endif

#define DRV_NAME        "tcc-multi-mbox-test"
#define DEF_FIFO_DEPTH  16 /* number of tcc_mbox_data elements */

static unsigned int fifo_depth = DEF_FIFO_DEPTH;
module_param(fifo_depth, uint, 0644);
MODULE_PARM_DESC(fifo_depth, "RX FIFO depth (messages)");

/* ----- Device context ----- */
struct mbox_test_dev {
	struct platform_device *pdev;
	struct device *dev;

	/* char dev */
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	const char *devname;     /* from DT: device-name */

	/* mailbox */
	struct mbox_client cl;
	struct mbox_chan *ch;
	const char *ch_name;     /* from DT: mbox-names */

	/* sync */
	struct mutex tx_lock;

	/* RX queue for userspace */
	wait_queue_head_t rx_wq;
	struct mutex rx_lock;
	struct kfifo rx_fifo; /* element size = struct tcc_mbox_data */

	/* wakeup state */
	bool wakeup_enabled;
};

/* ----- RX callback from mailbox framework ----- */
static void mbox_test_rx_cb(struct mbox_client *cl, void *message)
{
    struct platform_device *pdev = to_platform_device(cl->dev);
    struct mbox_test_dev *mdev = platform_get_drvdata(pdev);
    struct tcc_mbox_data *msg = message;
    unsigned int pushed;

    if (!mdev || !msg)
        return;

    /* NOTE:
     * deep-sleep에서의 웨이크업 자체는 컨트롤러 IRQ + DT(wakeup-source)에서 처리됨.
     * 여기서는 단순히 수신 큐에 적재하고 유저 공간을 깨운다.
     */

    mutex_lock(&mdev->rx_lock);
    pushed = kfifo_in(&mdev->rx_fifo, msg, sizeof(*msg));
    if (pushed != sizeof(*msg)) {
        dev_warn(mdev->dev, "rx fifo full/partial (%u/%zu), drop\n",
                 pushed, sizeof(*msg));
    } else {
        wake_up_interruptible(&mdev->rx_wq);
    }
    mutex_unlock(&mdev->rx_lock);
}


/* ----- TX helper ----- */
static int mbox_test_send(struct mbox_test_dev *mdev, const struct tcc_mbox_data *msg)
{
	int ret;

	if (!mdev || !msg)
		return -EINVAL;

	if (msg->data_len > MBOX_DATA_FIFO_SIZE)
		return -EINVAL;

	mutex_lock(&mdev->tx_lock);
	ret = mbox_send_message(mdev->ch, (void *)msg);
	/* mailbox core returns >=0 on success */
	ret = (ret >= 0) ? 0 : ret;
	mutex_unlock(&mdev->tx_lock);

	return ret;
}

/* ----- file operations ----- */
static int mbox_test_open(struct inode *inode, struct file *filp)
{
	struct mbox_test_dev *mdev =
		container_of(inode->i_cdev, struct mbox_test_dev, cdev);
	filp->private_data = mdev;
	return 0;
}

static int mbox_test_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* userspace writes a whole struct tcc_mbox_data */
static ssize_t mbox_test_write(struct file *filp, const char __user *buf,
                               size_t len, loff_t *ppos)
{
    struct mbox_test_dev *mdev = filp->private_data;
    struct tcc_mbox_data kmsg;
    size_t hdr_sz = sizeof(kmsg.cmd) + sizeof(kmsg.data_len); // 6*4 + 4 = 28 bytes
    size_t avail_words, to_copy_words;

    if (len < hdr_sz)
        return -EINVAL;

    /* 1) header first */
    if (copy_from_user(&kmsg, buf, hdr_sz))
        return -EFAULT;

    /* 2) payload len check */
    if (kmsg.data_len > MBOX_DATA_FIFO_SIZE)
        return -EINVAL;

    /* 3) calc actual available words from user */
    avail_words = (len - hdr_sz) / 4;
    to_copy_words = (kmsg.data_len < avail_words) ? kmsg.data_len : avail_words;

    /* 4) copy payload if present */
    if (to_copy_words) {
        if (copy_from_user(kmsg.data,
                           (const void __user *)((const char __user *)buf + hdr_sz),
                           to_copy_words * 4))
            return -EFAULT;
    }

    if (mbox_test_send(mdev, &kmsg))
        return -EIO;

    return hdr_sz + to_copy_words * 4;
}

/* userspace reads a whole struct tcc_mbox_data (blocking) */
static ssize_t mbox_test_read(struct file *filp, char __user *buf,
                              size_t len, loff_t *ppos)
{
    struct mbox_test_dev *mdev = filp->private_data;
    struct tcc_mbox_data kmsg;
    size_t hdr_sz = sizeof(kmsg.cmd) + sizeof(kmsg.data_len);
    unsigned int got;
    size_t msg_size;
    int ret;

    /* block until data arrives */
    ret = wait_event_interruptible(mdev->rx_wq,
                                   kfifo_len(&mdev->rx_fifo) >= sizeof(kmsg));
    if (ret)
        return ret;

    /* peek */
    mutex_lock(&mdev->rx_lock);
    got = kfifo_out_peek(&mdev->rx_fifo, &kmsg, sizeof(kmsg));
    if (got != sizeof(kmsg)) {
        mutex_unlock(&mdev->rx_lock);
        return -EAGAIN;
    }

    if (kmsg.data_len > MBOX_DATA_FIFO_SIZE) {
        mutex_unlock(&mdev->rx_lock);
        return -EIO;
    }
    msg_size = hdr_sz + ((size_t)kmsg.data_len * 4);

    if (len < msg_size) {
        mutex_unlock(&mdev->rx_lock);
        return -EMSGSIZE;
    }

    /* pop */
    got = kfifo_out(&mdev->rx_fifo, &kmsg, sizeof(kmsg));
    mutex_unlock(&mdev->rx_lock);

    if (got != sizeof(kmsg))
        return -EAGAIN;

    if (copy_to_user(buf, &kmsg, msg_size))
        return -EFAULT;

    return (ssize_t)msg_size;
}


static __poll_t mbox_test_poll(struct file *filp, poll_table *wait)
{
	struct mbox_test_dev *mdev = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &mdev->rx_wq, wait);

	if (kfifo_len(&mdev->rx_fifo) >= sizeof(struct tcc_mbox_data))
		mask |= POLLIN;
	mask |= POLLOUT;

	return mask;
}

static const struct file_operations mbox_test_fops = {
	.owner          = THIS_MODULE,
	.open           = mbox_test_open,
	.release        = mbox_test_release,
	.read           = mbox_test_read,
	.write          = mbox_test_write,
	.poll           = mbox_test_poll,
};

/* ----- PM ops (suspend/resume) ----- */
static int mbox_test_suspend(struct device *dev)
{
	return 0;
}

static int mbox_test_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mbox_test_pm_ops, mbox_test_suspend, mbox_test_resume);

/* ----- probe/remove ----- */
static int mbox_test_create_cdev(struct mbox_test_dev *mdev)
{
	int ret;

	ret = alloc_chrdev_region(&mdev->devt, 0, 1, mdev->devname);
	if (ret)
		return ret;

	mdev->class = class_create(THIS_MODULE, mdev->devname);
	if (IS_ERR(mdev->class)) {
		ret = PTR_ERR(mdev->class);
		goto err_unreg;
	}

	cdev_init(&mdev->cdev, &mbox_test_fops);
	mdev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&mdev->cdev, mdev->devt, 1);
	if (ret)
		goto err_class;

	mdev->dev = device_create(mdev->class, &mdev->pdev->dev,
	                          mdev->devt, NULL, "%s", mdev->devname);
	if (IS_ERR(mdev->dev)) {
		ret = PTR_ERR(mdev->dev);
		goto err_cdev;
	}
	return 0;

err_cdev:
	cdev_del(&mdev->cdev);
err_class:
	class_destroy(mdev->class);
err_unreg:
	unregister_chrdev_region(mdev->devt, 1);
	return ret;
}

static void mbox_test_destroy_cdev(struct mbox_test_dev *mdev)
{
	if (!mdev)
		return;
	device_destroy(mdev->class, mdev->devt);
	cdev_del(&mdev->cdev);
	class_destroy(mdev->class);
	unregister_chrdev_region(mdev->devt, 1);
}

static int mbox_test_probe(struct platform_device *pdev)
{
	struct mbox_test_dev *mdev;
	int ret;

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->pdev = pdev;
	platform_set_drvdata(pdev, mdev);

	/* DT properties */
	of_property_read_string(pdev->dev.of_node, "device-name", &mdev->devname);
	if (!mdev->devname)
		mdev->devname = DRV_NAME;

	of_property_read_string(pdev->dev.of_node, "mbox-names", &mdev->ch_name);
	if (!mdev->ch_name)
		mdev->ch_name = "tx";

	/* RX infra */
	init_waitqueue_head(&mdev->rx_wq);
	mutex_init(&mdev->rx_lock);
	mutex_init(&mdev->tx_lock);

	ret = kfifo_alloc(&mdev->rx_fifo,
	                  fifo_depth * sizeof(struct tcc_mbox_data),
	                  GFP_KERNEL);
	if (ret)
		return ret;

	/* mailbox client */
	mdev->cl.dev           = &pdev->dev;
	mdev->cl.rx_callback   = mbox_test_rx_cb;
	mdev->cl.tx_block      = true;
	mdev->cl.knows_txdone  = false;
	mdev->cl.tx_tout       = CLIENT_MBOX_TX_TIMEOUT; /* from Telechips headers */

	mdev->ch = mbox_request_channel_byname(&mdev->cl, mdev->ch_name);
	if (IS_ERR(mdev->ch)) {
		ret = PTR_ERR(mdev->ch);
		dev_err(&pdev->dev, "mbox_request_channel_byname(%s) failed: %d\n",
		        mdev->ch_name, ret);
		goto err_fifo;
	}

	ret = mbox_test_create_cdev(mdev);
	if (ret) {
		dev_err(&pdev->dev, "cdev create failed: %d\n", ret);
		goto err_mbox;
	}

	/* mark this device as wakeup-capable */
	device_init_wakeup(&pdev->dev, true);
	device_wakeup_enable(&pdev->dev);
	mdev->wakeup_enabled = true;

	dev_info(&pdev->dev, "ready: /dev/%s (channel \"%s\"), wakeup-capable\n",
	         mdev->devname, mdev->ch_name);
	return 0;

err_mbox:
	mbox_free_channel(mdev->ch);
err_fifo:
	kfifo_free(&mdev->rx_fifo);
	return ret;
}

static int mbox_test_remove(struct platform_device *pdev)
{
	struct mbox_test_dev *mdev = platform_get_drvdata(pdev);

	if (mdev && mdev->wakeup_enabled) {
		device_wakeup_disable(&pdev->dev);
		device_init_wakeup(&pdev->dev, false);
	}

	mbox_test_destroy_cdev(mdev);
	if (!IS_ERR(mdev->ch))
		mbox_free_channel(mdev->ch);
	kfifo_free(&mdev->rx_fifo);
	return 0;
}

static const struct of_device_id mbox_test_of_match[] = {
	{ .compatible = "telechips,mbox_test", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mbox_test_of_match);

static struct platform_driver mbox_test_driver = {
	.probe  = mbox_test_probe,
	.remove = mbox_test_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = mbox_test_of_match,
		.pm             = &mbox_test_pm_ops,
	},
};

module_platform_driver(mbox_test_driver);

MODULE_AUTHOR("Telechips / (modified for RW + wakeup)");
MODULE_DESCRIPTION("Telechips Multi-channel Mailbox RW client (wakeup-capable)");
MODULE_LICENSE("GPL");

/**
 * @file    panda.c
 * @author  Jeddy Diamond Exum
 * @date    16 June 2017
 * @version 0.1
 * @brief   Driver for the Comma.ai Panda CAN adapter to allow it to be controlled via
 * the Linux SocketCAN interface.
 * @see https://github.com/commaai/panda for the full project.
 */

#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/netdevice.h>
#include <linux/usb.h>

#include <linux/timer.h>

/* vendor and product id */
#define PANDA_MODULE_NAME "panda"
#define PANDA_VENDOR_ID 0XBBAA
#define PANDA_PRODUCT_ID 0XDDCC

// I don't get this yet
#define PANDA_MAX_TX_URBS 20
#define PANDA_USB_RX_BUFF_SIZE 0x40
#define PANDA_USB_TX_BUFF_SIZE (sizeof(struct panda_usb_can_msg))

#define PANDA_CAN_TRANSMIT 1
#define PANDA_CAN_EXTENDED 4

static const struct usb_device_id panda_usb_table[] = {
  { USB_DEVICE(PANDA_VENDOR_ID, PANDA_PRODUCT_ID) },
  {} /* Terminating entry */
};

struct panda_priv {
  struct can_priv can;
  struct usb_device *udev;
  struct net_device *netdev;
  struct usb_anchor tx_submitted;
  struct usb_anchor rx_submitted;

  unsigned int id;

  struct timer_list timer;
};

struct __packed panda_usb_can_msg {
  u32 rir;
  u32 bus_dat_len;
  u8 data[8];
};

struct panda_usb_ctx {
  struct panda_priv *priv;
};

unsigned int curr_id = 0;

static void panda_urb_unlink(struct panda_priv *priv)
{
  usb_kill_anchored_urbs(&priv->rx_submitted);
  usb_kill_anchored_urbs(&priv->tx_submitted);
}

static int panda_set_output_enable(struct panda_priv* priv, bool enable){
  return usb_control_msg(priv->udev, usb_sndctrlpipe(priv->udev, 0),
			 0xDC, USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			 enable ? 0x1337 : 0, 0, NULL, 0, USB_CTRL_SET_TIMEOUT);
}

//static int panda_write_can(struct panda_priv* priv, u8 *buf, unsigned int len, int *actual_len){
//  return usb_bulk_msg(priv->udev, usb_sndbulkpipe(priv->udev, 3),
//		      buf, len, actual_len, 5000);
//}

static void panda_usb_write_bulk_callback(struct urb *urb)
{
  struct panda_usb_ctx *ctx = urb->context;
  struct net_device *netdev;

  WARN_ON(!ctx);

  netdev = ctx->priv->netdev;

  /* free up our allocated buffer */
  usb_free_coherent(urb->dev, urb->transfer_buffer_length,
		    urb->transfer_buffer, urb->transfer_dma);

  //if (ctx->can) {
  //  if (!netif_device_present(netdev))
  //    return;
  //
  //  netdev->stats.tx_packets++;
  //  netdev->stats.tx_bytes += ctx->dlc;
  //
  //  can_led_event(netdev, CAN_LED_EVENT_TX);
  //  can_get_echo_skb(netdev, ctx->ndx);
  //}

  if (urb->status)
    netdev_info(netdev, "Tx URB aborted (%d)\n", urb->status);

  printk("PANDA SENT OUT DATA\n");

  /* Release the context */
  //mcba_usb_free_ctx(ctx);
}


static netdev_tx_t panda_usb_xmit(struct panda_priv *priv,
				  struct panda_usb_can_msg *usb_msg,
				  struct panda_usb_ctx *ctx)
{
  struct urb *urb;
  u8 *buf;
  int err;

  /* create a URB, and a buffer for it, and copy the data to the URB */
  urb = usb_alloc_urb(0, GFP_ATOMIC);
  if (!urb)
    return -ENOMEM;

  buf = usb_alloc_coherent(priv->udev, PANDA_USB_TX_BUFF_SIZE, GFP_ATOMIC,
			   &urb->transfer_dma);
  if (!buf) {
    err = -ENOMEM;
    goto nomembuf;
  }

  memcpy(buf, usb_msg, PANDA_USB_TX_BUFF_SIZE);

  usb_fill_bulk_urb(urb, priv->udev,
		    usb_sndbulkpipe(priv->udev, 3), buf,
		    PANDA_USB_TX_BUFF_SIZE, panda_usb_write_bulk_callback,
		    priv);//ctx);

  urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
  usb_anchor_urb(urb, &priv->tx_submitted);

  err = usb_submit_urb(urb, GFP_ATOMIC);
  if (unlikely(err))
    goto failed;

  /* Release our reference to this URB, the USB core will eventually free it entirely. */
  usb_free_urb(urb);

  return 0;

 failed:
  usb_unanchor_urb(urb);
  usb_free_coherent(priv->udev, PANDA_USB_TX_BUFF_SIZE, buf, urb->transfer_dma);

  if (err == -ENODEV)
    netif_device_detach(priv->netdev);
  else
    netdev_warn(priv->netdev, "failed tx_urb %d\n", err);

 nomembuf:
  usb_free_urb(urb);

  return err;
}


static netdev_tx_t panda_usb_start_xmit(struct panda_priv *priv, u16 addr, u16 bus,
					u8 *dat, unsigned int len)
//struct sk_buff *skb, struct net_device *netdev)
{
  //struct panda_priv *priv = netdev_priv(netdev);
  //struct can_frame *cf = (struct can_frame *)skb->data;
  struct panda_usb_ctx *ctx = NULL;
  //struct net_device_stats *stats = &priv->netdev->stats;
  //u16 sid;
  int err;
  struct panda_usb_can_msg usb_msg = {};

  //if (can_dropped_invalid_skb(netdev, skb))
  //  return NETDEV_TX_OK;

  //ctx = panda_usb_get_free_ctx(priv, cf);
  //if (!ctx)
  //  return NETDEV_TX_BUSY;

  //can_put_echo_skb(skb, priv->netdev, ctx->ndx);

  if(addr >= 0x800){
    usb_msg.rir = cpu_to_le32((addr << 3) | PANDA_CAN_TRANSMIT | PANDA_CAN_EXTENDED);
  }else{
    usb_msg.rir = cpu_to_le32((addr << 21) | PANDA_CAN_TRANSMIT);
  }
  usb_msg.bus_dat_len = cpu_to_le32(len | (bus << 4));

  //usb_msg.dlc = cf->can_dlc;

  //memcpy(usb_msg.data, cf->data, usb_msg.dlc);
  memcpy(usb_msg.data, dat, len);

  //if (cf->can_id & CAN_RTR_FLAG)
  //  usb_msg.dlc |= PANDA_DLC_RTR_MASK;

  err = panda_usb_xmit(priv, (struct panda_usb_can_msg *)&usb_msg, ctx);
  if (err)
    goto xmit_failed;

  return NETDEV_TX_OK;

 xmit_failed:
  //can_free_echo_skb(priv->netdev, ctx->ndx);
  //panda_usb_free_ctx(ctx);
  //dev_kfree_skb(skb);
  //stats->tx_dropped++;

  return NETDEV_TX_OK;
}

static void panda_usb_read_int_callback(struct urb *urb)
{
  struct panda_priv *priv = urb->context;
  struct net_device *netdev;
  int retval;
  int pos = 0;
  int num_recv = 0;

  netdev = priv->netdev;

  //if (!netif_device_present(netdev))
  //  return;

  switch (urb->status) {
  case 0: /* success */
    break;
  case -ENOENT:
  case -ESHUTDOWN:
    return;
  default:
    netdev_info(netdev, "Rx URB aborted (%d)\n", urb->status);
    goto resubmit_urb;
  }

  while (pos < urb->actual_length) {
    struct panda_usb_can_msg *msg;

    if (pos + sizeof(struct panda_usb_can_msg) > urb->actual_length) {
      netdev_err(priv->netdev, "format error\n");
      break;
    }

    msg = (struct panda_usb_can_msg *)(urb->transfer_buffer + pos);

    num_recv++;
    //panda_usb_process_rx(priv, msg);

    pos += sizeof(struct panda_usb_can_msg);
  }

  netdev_info(netdev, "Received (%d) entries\n", num_recv);

 resubmit_urb:
  usb_fill_int_urb(urb, priv->udev,
		    usb_rcvintpipe(priv->udev, 1),
		    urb->transfer_buffer, PANDA_USB_RX_BUFF_SIZE,
		    panda_usb_read_int_callback, priv, 10);

  retval = usb_submit_urb(urb, GFP_ATOMIC);

  if (retval == -ENODEV)
    netif_device_detach(netdev);
  else if (retval)
    netdev_err(netdev, "failed resubmitting read bulk urb: %d\n", retval);
}


static int panda_usb_start(struct panda_priv *priv)
{
  struct net_device *netdev = priv->netdev;
  int err;
  struct urb *urb = NULL;
  u8 *buf;

  /* create a URB, and a buffer for it */
  urb = usb_alloc_urb(0, GFP_KERNEL);
  if (!urb) {
    return -ENOMEM;
  }

  buf = usb_alloc_coherent(priv->udev, PANDA_USB_RX_BUFF_SIZE,
			   GFP_KERNEL, &urb->transfer_dma);
  if (!buf) {
    netdev_err(netdev, "No memory left for USB buffer\n");
    usb_free_urb(urb);
    return -ENOMEM;
  }

  usb_fill_int_urb(urb, priv->udev,
                   usb_rcvintpipe(priv->udev, 1),
                   buf, PANDA_USB_RX_BUFF_SIZE,
                   panda_usb_read_int_callback, priv, 10);
  urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
  usb_anchor_urb(urb, &priv->rx_submitted);

  err = usb_submit_urb(urb, GFP_KERNEL);
  if (err) {
  usb_unanchor_urb(urb);
    usb_free_coherent(priv->udev, PANDA_USB_RX_BUFF_SIZE, buf, urb->transfer_dma);
    usb_free_urb(urb);
    netdev_err(netdev, "Failed in start, while submitting urb.\n");
    return err;
  }

  /* Drop reference, USB core will take care of freeing it */
  usb_free_urb(urb);


  return 0;
}


//static u8 outdat[] = {0x01, 0x00, 0x40, 0x15, 0x08, 0x00, 0x00, 0x00,
//		      0xAA, 0xAA, 0xAA, 0xAA, 0x07, 0x00, 0x00, 0x00};
static u8 payload[] = {0xAA, 0xAA, 0xAA, 0xAA, 0x07, 0x00, 0x00, 0x00};

void my_timer_callback( unsigned long data )
{
  int err;//, actual_size;
  struct panda_priv *priv = (struct panda_priv *) data;

  err = panda_usb_start_xmit(priv, 0xAA, 0, payload, 8);
  //err = panda_write_can(priv, outdat, sizeof(outdat), &actual_size);
  if(err != NETDEV_TX_OK){
    printk("PANDA TIMER failed to do usb thing. Err: %d\n", err);
  }
  //}else{
  //  printk("Sent out %d bytes on timer\n", actual_size);
  //}

  mod_timer(&priv->timer, jiffies + msecs_to_jiffies(1000));
}

static int panda_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
  struct net_device *netdev;
  struct panda_priv *priv;
  int err = -ENOMEM;
  struct usb_device *usbdev = interface_to_usbdev(intf);

  netdev = alloc_candev(sizeof(struct panda_priv), PANDA_MAX_TX_URBS);
  if (!netdev) {
    dev_err(&intf->dev, "Couldn't alloc candev\n");
    return -ENOMEM;
  }

  priv = netdev_priv(netdev);

  priv->udev = usbdev;
  priv->netdev = netdev;
  priv->id = curr_id++;

  init_usb_anchor(&priv->rx_submitted);
  init_usb_anchor(&priv->tx_submitted);

  usb_set_intfdata(intf, priv);

  /* Init CAN device */
  priv->can.state = CAN_STATE_STOPPED;

  //err = register_candev(netdev);
  //if (err) {
  //  netdev_err(netdev, "couldn't register CAN device: %d\n", err);
  //  goto cleanup_free_candev;
  //}

  err = panda_usb_start(priv);
  if (err) {
    dev_info(&intf->dev, "Failed to initialize Comma.ai Panda CAN controller\n");
    goto cleanup_unregister_candev;
  }

  dev_info(&intf->dev, "Comma.ai Panda CAN controller connected (ID: %u)\n", priv->id);

  err = panda_set_output_enable(priv, true);
  if (err) {
    dev_info(&intf->dev, "Failed to initialize send enable message to Panda.\n");
    goto cleanup_unregister_candev;
  }

  // 0x01, 0x00, 0x40, 0x15, 0x08, 0x00, 0x00, 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0x07, 0x00, 0x00, 0x00

  setup_timer(&priv->timer, my_timer_callback, (unsigned long) priv);
  mod_timer(&priv->timer, jiffies + msecs_to_jiffies(1000));

  return 0;

 cleanup_unregister_candev:
  //unregister_candev(priv->netdev);
  goto cleanup_free_candev;

 cleanup_free_candev:
  free_candev(priv->netdev);

  return err;
}

/* Called by the usb core when driver is unloaded or device is removed */
static void panda_usb_disconnect(struct usb_interface *intf)
{
  struct panda_priv *priv = usb_get_intfdata(intf);

  usb_set_intfdata(intf, NULL);

  dev_info(&intf->dev, "Removed Comma.ai Panda CAN controller (ID: %u)\n", priv->id);

  del_timer(&priv->timer);

  //unregister_candev(priv->netdev);
  free_candev(priv->netdev);

  panda_urb_unlink(priv);
}

static struct usb_driver panda_usb_driver = {
  .name = PANDA_MODULE_NAME,
  .probe = panda_usb_probe,
  .disconnect = panda_usb_disconnect,
  .id_table = panda_usb_table,
};

module_usb_driver(panda_usb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jessy Diamond Exum <jessy.diamondman@gmail.com>");
MODULE_DESCRIPTION("SocketCAN driver for Comma.ai's Panda Adapter.");
MODULE_VERSION("0.1");
MODULE_DEVICE_TABLE(usb, panda_usb_table);

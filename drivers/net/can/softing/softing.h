/*
 * softing common interfaces
 *
 * by Kurt Van Dijck, 06-2008
 */

#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/can.h>
#include <linux/can/dev.h>

struct softing;
struct sofing_desc;

/* special attribute, so we should not rely on the ->priv pointers
 * before knowing how to interpret these
 */
struct softing_attribute;

struct softing_priv {
	struct can_priv can;	/* must be the first member! */
	struct net_device *netdev;
	struct softing *card;
	struct can_bittiming_const softing_bittiming;
	struct {
		int pending;
		/* variables wich hold the circular buffer */
		int echo_put;
		int echo_get;
	} tx;
	int index;
	u8 sample;
	u8 output;
	u16 chip;
	struct attribute_group sysfs;
};
#define netdev2softing(netdev)	((struct softing_priv *)netdev_priv(netdev))

struct softing_desc {
	unsigned int manf;
	unsigned int prod;
	/* generation
	 * 1st with NEC or SJA1000
	 * 8bit, exclusive interrupt, ...
	 * 2nd only SJA11000
	 * 16bit, shared interrupt
	 */
	int generation;
	unsigned int freq;	/*crystal in MHz */
	unsigned int max_brp;
	unsigned int max_sjw;
	unsigned long dpram_size;
	char name[32];
	struct {
		unsigned long offs;
		unsigned long addr;
		char fw[32];
	} boot, load, app;
};

struct softing {
	int nbus;
	struct softing_priv *bus[2];
	spinlock_t	 spin; /* protect this structure & DPRAM access */

	struct {
		/* indication of firmware status */
		int up;
		/* protection of the 'up' variable */
		struct mutex lock;
	} fw;
	struct {
		int nr;
		int shared;
		int requested;
		struct tasklet_struct bh;
		int svc_count;
	} irq;
	struct {
		int pending;
		int last_bus;
		/* keep the bus that last tx'd a message,
		 * in order to let every netdev queue resume
		 */
	} tx;
	struct {
		unsigned long phys;
		unsigned long size;
		unsigned char *virt;
		unsigned char *end;
		struct softing_fct  *fct;
		struct softing_info *info;
		struct softing_rx  *rx;
		struct softing_tx  *tx;
		struct softing_irq *irq;
		unsigned short *command;
		unsigned short *receipt;
	} dpram;
	struct {
		unsigned short manf;
		unsigned short prod;
		u32  serial, fw, hw, lic;
		u16  chip [2];
		u32  freq;
		const char *name;
	} id;
	const struct softing_desc		*desc;
	struct {
		int (*reset)	 (struct softing *, int);
		int (*enable_irq)(struct softing *, int);
	} fn;
	struct device *dev;
	/* sysfs */
	struct attribute_group sysfs;
	struct softing_attribute *attr;
	struct attribute **grp;
};

extern int	mk_softing(struct softing *);
/* fields that must be set already are :
 * ncan
 * id.manf
 * id.prod
 * fn.reset
 * fn.enable_irq
 */
extern void rm_softing(struct softing *);
/* usefull functions during operation */

extern const struct softing_desc *
	softing_lookup_desc(unsigned int manf, unsigned int prod);

extern int softing_default_output(struct softing *card
			, struct softing_priv *priv);
extern u32 softing_time2usec(struct softing *card, u32 raw);

extern int softing_fct_cmd(struct softing *card
			, int cmd, int vector, const char *msg);

extern int softing_bootloader_command(struct softing *card
			, int command, const char *msg);

/* Load firmware after reset */
extern int softing_load_fw(const char *file, struct softing *card,
			unsigned char *virt, unsigned int size, int offset);

/* Load final application firmware after bootloader */
extern int softing_load_app_fw(const char *file, struct softing *card);

extern int softing_reset_chip(struct softing *card);

/* enable or disable irq
 * only called with fw.lock locked
 */
extern int softing_card_irq(struct softing *card, int enable);

/* called when tx queue is flushed */
extern void softing_flush_echo_skb(struct softing_priv *priv);

/* reinitaliase the card, apply -1 for bus[01] for 'no change' */
extern int softing_reinit(struct softing *card, int bus0, int bus1);

/* SOFTING DPRAM mappings */
struct softing_rx {
	u8  fifo[16][32];
	u8  dummy1;
	u16 rd;
	u16 dummy2;
	u16 wr;
	u16  dummy3;
	u16 lost_msg;
} __attribute__((packed));

#define TXMAX	31
struct softing_tx {
	u8  fifo[32][16];
	u8  dummy1;
	u16 rd;
	u16 dummy2;
	u16 wr;
	u8  dummy3;
} __attribute__((packed));

struct softing_irq {
	u8 to_host;
	u8 to_card;
} __attribute__((packed));

struct softing_fct {
	s16 param[20]; /* 0 is index */
	s16 returned;
	u8  dummy;
	u16 host_access;
} __attribute__((packed));

struct softing_info {
	u8  dummy1;
	u16 bus_state;
	u16 dummy2;
	u16 bus_state2;
	u16 dummy3;
	u16 error_state;
	u16 dummy4;
	u16 error_state2;
	u16 dummy5;
	u16 reset;
	u16 dummy6;
	u16 clear_rcv_fifo;
	u16 dummy7;
	u16 dummyxx;
	u16 dummy8;
	u16 time_reset;
	u8  dummy9;
	u32 time;
	u32 time_wrap;
	u8  wr_start;
	u8  wr_end;
	u8  dummy10;
	u16 dummy12;
	u16 dummy12x;
	u16 dummy13;
	u16 reset_rcv_fifo;
	u8  dummy14;
	u8  reset_xmt_fifo;
	u8  read_fifo_levels;
	u16 rcv_fifo_level;
	u16 xmt_fifo_level;
} __attribute__((packed));

/* DPRAM return codes */
#define RES_NONE 0
#define RES_OK	 1
#define RES_NOK  2
#define RES_UNKNOWN 3
/* DPRAM flags */
#define CMD_TX		0x01
#define CMD_ACK 0x02
#define CMD_XTD 0x04
#define CMD_RTR 0x08
#define CMD_ERR 0x10
#define CMD_BUS2	0x80

/* debug */
extern int softing_debug;

#define mod_alert(fmt,arg...) { \
	if (softing_debug >= 0) \
		printk(KERN_ALERT "[%s] %s:" fmt "\n" \
			, THIS_MODULE->name \
			, __func__ \
			, ##arg); \
	}
#define mod_info(fmt,arg...) { \
	if (softing_debug >= 1) \
		printk(KERN_INFO	"[%s] %s:" fmt "\n"\
			, THIS_MODULE->name \
			, __func__ \
			, ##arg); \
	}
#define mod_trace(fmt,arg...) { \
	if (softing_debug >= 2) \
		printk(KERN_DEBUG "[%s] %s:" fmt "\n" \
			, THIS_MODULE->name \
			, __func__ \
			, ##arg); \
	}


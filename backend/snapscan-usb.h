#ifndef snapscan_usb_h
#define snapscan_usb_h

static SANE_Status snapscani_usb_cmd(int fd, const void *src, size_t src_size,
			 void *dst, size_t * dst_size);
static SANE_Status snapscani_usb_open(const char *dev, int *fdp,
                 SANEI_SCSI_Sense_Handler handler, void *handler_arg);
static void snapscani_usb_close(int fd);

/*
 *  USB status codes
 */
#define GOOD                 0x00
#define CHECK_CONDITION      0x01
#define CONDITION_GOOD       0x02
#define BUSY                 0x04
#define INTERMEDIATE_GOOD    0x08
#define INTERMEDIATE_C_GOOD  0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED   0x11
#define QUEUE_FULL           0x14

#define STATUS_MASK          0x3e

/*
 *  USB transaction status
 */
#define TRANSACTION_COMPLETED 0xfb   /* Scanner considers the transaction done */
#define TRANSACTION_READ 0xf9        /* Scanner has data to deliver */
#define TRANSACTION_WRITE 0xf8       /* Scanner is expecting more data */

/*
 *  Busy queue data structure and prototypes
 */
struct usb_busy_queue {
    int fd;
    void *src;
    size_t src_size;
    struct usb_busy_queue *next;
};

static struct usb_busy_queue *bqhead,*bqtail;
extern int bqelements;
static int enqueue_bq(int fd,const void *src, size_t src_size);
static void dequeue_bq(void);
static int is_queueable(const char *src);

static SANE_Status atomic_usb_cmd(int fd, const void *src, size_t src_size,
		    void *dst, size_t * dst_size);
static SANE_Status usb_open(const char *dev, int *fdp,
                 SANEI_SCSI_Sense_Handler handler, void *handler_arg);

static void usb_close(int fd);

static SANE_Status usb_cmd(int fd, const void *src, size_t src_size,
		    void *dst, size_t * dst_size);

#endif

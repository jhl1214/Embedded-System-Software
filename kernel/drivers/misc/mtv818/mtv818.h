#ifndef __MTV818_H__
#define __MTV818_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/atomic.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>  
#include <linux/list.h> 
#include <linux/freezer.h>
#include <linux/completion.h>
#include <linux/smp_lock.h>

#include "./src/raontv.h"
#include "mtv818_ioctl.h"



#define MTV818_NON_BLOCKING_READ_MODE

#define DMB_DEBUG


#define DMBERR(args...)		do { printk(KERN_ERR  args); } while (0)

#ifdef DMB_DEBUG
	#define DMBMSG(args...)	do { printk(KERN_INFO args); } while (0)
#else 
	#define DMBMSG(x...)  /* null */
#endif 


#define MAX_NUM_TS_PKT_BUF 	40


#if defined(RTV_TDMB_ENABLE) && defined(RTV_CIF_MODE_ENABLED)
	#define MTV_TS_THRESHOLD_SIZE		(20 * 188) // Includeing CIF Header
	
#else
	#define MTV_TS_THRESHOLD_SIZE		(6*188)
#endif	


typedef struct
{
	struct list_head link; // to use queuing
	UINT len;    
	unsigned char msc_buf[MTV_TS_THRESHOLD_SIZE + 1];
} MTV818_TS_PKT_INFO; 



/* Control Block */
struct mtv818_cb
{
	E_DMB_TV_MODE_TYPE tv_mode; 
	E_RTV_COUNTRY_BAND_TYPE country_band_type;
	BOOL is_power_on;
	volatile int stop;	
		
	struct device *dev;

#if defined(RTV_IF_SPI) || (defined(RTV_TDMB_ENABLE) && !defined(RTV_TDMB_FIC_POLLING_MODE))	
	struct task_struct *isr_thread_cb;
	wait_queue_head_t isr_wq;
	unsigned int isr_cnt;	
#endif	

#ifdef RTV_FM_ENABLE
	E_RTV_ADC_CLK_FREQ_TYPE adc_clk_type;
#endif

#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)
	struct i2c_client *i2c_client_ptr;
	struct i2c_adapter *i2c_adapter_ptr;
   
#elif defined(RTV_IF_SPI)
	struct spi_device *spi_ptr;
	
	wait_queue_head_t read_wq;
	MTV818_TS_PKT_INFO *prev_tsp; // previous tsp
	unsigned int prev_org_tsp_size;
	struct completion read_exit;
	
  #ifdef RTV_DUAL_CHIP_USED
  	struct spi_device *spi_slave_ptr;
  #endif
#endif
};


extern  struct mtv818_cb *mtv818_cb_ptr;



typedef struct
{
	struct list_head head;
	volatile unsigned int cnt; // queue count
	unsigned int total_bytes;
	spinlock_t lock;
} MTV818_TSP_QUEUE_INFO;

unsigned int mtv818_get_total_tsp(void);
void mtv818_reset_tsp(void);
MTV818_TS_PKT_INFO *mtv818_get_tsp(void);
void mtv818_put_tsp(MTV818_TS_PKT_INFO *pkt);
void mtv818_free_tsp(MTV818_TS_PKT_INFO *pkt);
MTV818_TS_PKT_INFO *mtv818_alloc_tsp(void);



#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV818_H__*/

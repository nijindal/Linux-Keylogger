#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include<linux/interrupt.h>

MODULE_LICENSE("GPL");

//Prototypes − this would normally go in a .h file
#define DEVICE_NAME "KEY_LOGGER" /* Dev name as it appears in /proc/devices */
#define SUCCESS 0

dev_t dev_no;

struct logger
   {
        char code;
		struct cdev cdev;
   }key_logger;

static int wake_up_variable = 0; 
char modname[] = "my_key_logger";
static int ch = 0;

DECLARE_WAIT_QUEUE_HEAD(wait_queue);

int my_proc_read( char *buff, char **start, off_t offp, int count, int *eof, void *data ) 
                                                                                          
   {
        int len,val;
        len = 0;
        val = 0;
        printk("\n..................... In 'my_proc_read' Function ............................");    // for displaying this line in 'dmesg' .....
        printk("\n The value Of len (In The Beginning) : %d",len);   

        len += sprintf( buff+len, "\n\n Getting the info for debugging from kernel .... \n ");
        printk("\n Name Of The Device : %s",modname);

        val = copy_to_user(buff,modname,count);

        printk("\n\n The value of len : %d",len);
        if(val <= 0)
             {
                printk("\n The 'copy_from_user()' function returned a value <= 0... This means error in copying  ...");
                return -EFAULT;
             }

        printk("\n The Buffer Content : %s",buff);
        printk("\n The Data Content Read : %s",modname);
        printk("\n The value of len (In the End) : %d \n",len);
        printk("\n ................ Exitting From 'my_proc_read' Function ......................");

        return  len;
   }

int my_proc_write(struct file *filep, const char *buff, unsigned long count, void *data)
   {
//        char *choice;
        printk("\n................... In 'my_proc_write' Function ...................................");
        printk("\n The Value Of Count (In The Beginning) : %ld",count);
        printk("\n The Buffer Recieved (In The Beginning) : %s",buff);
        
        ch = (int)*buff;
        ch -= 48; 
        printk("\n The value of ch : %d ",ch);

        printk("\n........................... Exitting From 'my_proc_write' Function .........................");

        return count;
   }


ssize_t read_key_log(struct file *filep, char __user *buf, size_t count,loff_t *f_pos)
{
        struct logger *abc = (struct logger *)(filep->private_data);
	wake_up_variable = 0;

        printk("\n Going To Sleep .... \n");
	
        if(wake_up_variable == 0)
	   {
		if (wait_event_interruptible(wait_queue,(wake_up_variable == 1) )) // returns 0 when the condition becomes true ....
                        return -ERESTARTSYS;
		else
		   {	
			copy_to_user(buf,&(abc->code), 1);
			printk(KERN_WARNING"\nIn read key_log\n");
//                        printk("Buffer : %s ",buf);
			wake_up_variable = 0;
			return 1;	
		   }
	   }
	return 0;
}


struct file_operations fops = {
      .owner   = THIS_MODULE,
      .read    = read_key_log,
};   

void my_func(unsigned long data)
   {
        struct logger *abc;
	printk("\n I'm in my func \n");
        printk("\n The data received here : %lu \n",data);
        abc = (struct logger *)data;

        printk("\n The Address in 'abc' : %p " ,abc);
        printk("\n 'abc->code' : %c",abc->code);
        printk("\n Exitting From 'my_func' function ...");
   } 

// struct tasklet_struct my_tasklet = { NULL, 0, ATOMIC_INIT(0), my_func,(unsigned long)key_logger };
DECLARE_TASKLET(my_tasklet,my_func,(unsigned long)&key_logger);

// My Handler ....
// An interrupt service routine (ISR) is a software routine that hardware invokes in response to an interrupt
irqreturn_t key_logger_isr(int i,void *devid)        // here 'devid' received starting address of 'key_logger' ... 
   {                                                  
        struct logger *abc;
	        
	abc = (struct logger *)devid;
        printk("\n\n Inside 'key_logger_isr' function ...");
//	printk("\n CURRENT IN ISR,  wake up variable = %d\n",wake_up_variable);
        abc->code = inb(0x60);
       
        printk("Scan Code %x %s.\n",
          (int) *((char *) abc->code) & 0x7F,
          *((char *) abc->code) & 0x80 ? "Released" : "Pressed");
        printk("\n Read Code : %d ",abc->code);
        printk("\n Irq No. : %d   devid : %p   abc->code : %d",i,devid,abc->code);
        printk("\n The Address Of 'abc': %p ch : %d ",abc, ch);

	wake_up_variable = 1;
	wake_up_interruptible(&wait_queue);

//        printk("\n Do You Want To Schedule The Tasklet : ");
        if(ch)
           tasklet_schedule(&my_tasklet);

        printk("\n Exitting From 'key_logger_isr' function ...");
	return IRQ_HANDLED;
   }

int __init init_key_logger(void)
   { 
	int result, err;
	static struct proc_dir_entry *my_file;
	result = alloc_chrdev_region(&dev_no,0,1,DEVICE_NAME);  // registers a range of char device numbers and returns an integer
	if(result < 0)                                          // type number. The major number is chosen dynamically and is 
	    {                                                   // allocated to 'dev_no' ....              
		printk("Can't get device number ");
		return result;
	    }

        printk("\n Inside 'init_key_logger' function ...");                                                                  
		my_file = create_proc_entry(modname,0777,NULL);                    // 'NULL' ensures a proc entry in /proc ......
                                                                           // otherwise the parent directory can also be specified .....
        my_file->read_proc = my_proc_read;
        my_file->write_proc = my_proc_write;

//     int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *devname, void *dev_id);        
        request_irq(1,key_logger_isr,IRQF_SHARED,"i8042",&key_logger);     //created a filled node and assigned at given irq number ...

	cdev_init(&key_logger.cdev,&fops);          // initialize a cdev structure contained in 'key_logger' 
                                                     // making it ready to add to the system with cdev_add(). ...
        key_logger.cdev.owner = THIS_MODULE;        
        key_logger.cdev.ops = &fops;                // assigns the operations 'fops' to the 'key_logger->cdev.ops' ....
        err = cdev_add(&key_logger.cdev, dev_no, 1);

        /* Fail gracefully if need be */
        if (err)
		 printk(KERN_NOTICE "Error %d adding key logger ", err);
        printk("\n Exitting From 'init_key_logger' function ...");
        
//        kfree(key_logger);
	return SUCCESS;
   }


void __exit exit_key_logger(void)
{
        printk("\n Inside 'exit_key_logger' function ....");
//   Unregister the device and free IRQ line
        tasklet_kill(&my_tasklet);
        free_irq(1,&key_logger);
        remove_proc_entry( modname, NULL ); 
   	unregister_chrdev_region(dev_no,0);
        printk("\n Exitting From 'exit_key_logger' function ...");
}

module_init(init_key_logger);
module_exit(exit_key_logger);

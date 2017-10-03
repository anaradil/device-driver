/*
**	This "device" reverses strings that are written to it
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/types.h>   // for dev_t typedef
#include <linux/kdev_t.h>  // for format_dev_t
#include <linux/fs.h>      // for alloc_chrdev_region()
#include <asm/uaccess.h>   // for copy_to_user, copy_from_user

#define BUFSZ 256
#define PSDSZ 10

static dev_t mydev;             // (major,minor) value
static struct cdev my_cdev;
static int read_count = 0;
static int write_count = 0;
static int access_count = 0;
static int start;
static int end;
static char internal_buffer[BUFSZ];
static char password_buffer[PSDSZ];
static char state[256];
static struct semaphore r_sema, w_sema;
static int pass = 0;
/*********************************************************************
**	The set of file operations that are implemented in this driver
**********************************************************************/
static ssize_t device_read(struct file *filp, char __user *buf, 
			size_t len, loff_t *f_pos);
static ssize_t device_write(struct file *filp, const char __user *buf, 
			size_t len, loff_t *f_pos);

static int device_open( struct inode *, struct file *);
static int device_close( struct inode *, struct file *);

static struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = device_open, 
    .read = device_read,
    .write = device_write,
    .release = device_close
};

// Key Scheduling Algorithm 
// Input: state - the state used to generate the keystream
//        key - Key to use to initialize the state 
//        len - length of key in bytes  
void ksa(unsigned char state[], unsigned char key[], int len)
{
   int i,j=0,t; 
   
   for (i=0; i < 256; ++i)
      state[i] = i; 
   for (i=0; i < 256; ++i) {
      j = (j + state[i] + key[i % len]) % 256; 
      t = state[i]; 
      state[i] = state[j]; 
      state[j] = t; 
   }   
}

// Pseudo-Random Generator Algorithm 
// Input: state - the state used to generate the keystream 
//        out - Must be of at least "len" length
//        len - number of bytes to generate 
void prga(unsigned char state[], unsigned char out[], int len)
{  
   int i=0,j=0,x,t; 
   unsigned char key; 
   
   for (x=0; x < len; ++x)  {
      i = (i + 1) % 256; 
      j = (j + state[i]) % 256; 
      t = state[i]; 
      state[i] = state[j]; 
      state[j] = t; 
      out[x] = state[(state[i] + state[j]) % 256];
   }   
} 

/*********************************************************************
**	Called when the device is opened (system call open)
**********************************************************************/
static int device_open(struct inode *inodep, struct file *filep)
{
	if(filep->f_mode & FMODE_READ && read_count != 0)
	{
		return -EPERM;
	}
	if(filep->f_mode & FMODE_WRITE && write_count != 0)
	{
		return -EPERM;
	}

	if (filep->f_mode & FMODE_READ && read_count == 0)
	{
		read_count++;
		
	}
	if (filep->f_mode & FMODE_WRITE && write_count == 0)
	{
		write_count++;
	} 

	access_count++;
	printk(KERN_ALERT "Reverse device opened %d time(s)\n", access_count);
	return 0;
}


/**********************************************************************
**	Called when the device is read from
**********************************************************************/
static ssize_t device_read( struct file *filp, char __user *buf, 
				size_t len, loff_t *f_pos )
{
	// print some information
	printk( KERN_INFO "In reverse read routine.\n" );
	printk( KERN_INFO "Length field is %lu.\n", (long unsigned int) len );
	printk( KERN_INFO "Offset is %lu.\n", (long unsigned int) *f_pos );
	
	if(start == end)
	{
		printk( KERN_INFO "Error.\n" );
		return -EPERM;
	}
	
	int num = 0;
	while(len > 0)
	 {
		if(down_interruptible(&r_sema) != 0) {
			return -EFAULT;
		}
		unsigned char enchar[1];
		prga(state, enchar, 1);
		internal_buffer[start] = (char)((int)internal_buffer[start]) ^ ((int)enchar[0]);		
		if ( copy_to_user( buf + num, &internal_buffer[start], 1 ) != 0 )
			return -EFAULT;
		num++;
		len--;
		internal_buffer[start] = 0;
		start = (start + 1) % BUFSZ;
		up(&w_sema);
		
	}
	
	return num;     
}


/**********************************************************************
**	Called when the device is written to
**********************************************************************/
static ssize_t device_write( struct file *filep, const char *buf, 
				size_t len, loff_t *f_pos )
{
	int i = 0;   
	int count = 0;

	// print some information
	printk( KERN_INFO "In reverse write routine.\n" );
	printk( KERN_INFO "Length field is %lu.\n", (long unsigned int) len );
	printk( KERN_INFO "Offset is %lu.\n", (long unsigned int) *f_pos );

	if(pass == 0) {
		if(copy_from_user(password_buffer, buf, 10) != 0) {
			return -EFAULT;
		}
		pass = 1;
		return 10;
	}
	
	if((end + 1) % BUFSZ == start) {
		return -EPERM;
	}

	while ( len > 0 )
	{
		
		if(down_interruptible(&w_sema) != 0) {
			return -EFAULT;
		}
		if ( copy_from_user( &internal_buffer[end], buf+count, 1 ) != 0 )
			return -EFAULT;
		end = (end + 1) % BUFSZ;
		count++;
		len--;
		up(&r_sema);
		
	}
	return count;
}

/**********************************************************************
**	Called when the device is closed
**********************************************************************/
static int device_close(struct inode *inodep, struct file *filep)
{
   printk(KERN_ALERT "reverse device closed !\n");
	if(!(filep->f_mode & FMODE_WRITE) && read_count == 1)
	{
		read_count--;
		printk( KERN_INFO "Closed read %d.\n", read_count);
	}
		
	if(!(filep->f_mode & FMODE_READ) &&write_count == 1)
	{
		write_count--;
		printk( KERN_INFO "Closed write %d.\n", write_count);
	}
   return 0;
}


/**********************************************************************
**	Called when this driver module is loaded
**********************************************************************/
static int __init chardrv_in(void)
{
	sema_init(&r_sema, 0);
	sema_init(&w_sema, BUFSZ - 1);
	ksa(state, password_buffer, 10);
	start = 0;
	end = 0;
	int result;
	printk(KERN_INFO "module reverse driver being loaded.\n");

	result = alloc_chrdev_region(&mydev, 0, 1, "rday");
	if ( result < 0 )
	{
    		printk(KERN_WARNING "Failed to allocate major/minor numbers");
    		return result;
	}

	cdev_init(&my_cdev, &my_fops);
	my_cdev.owner = THIS_MODULE;
	result = cdev_add(&my_cdev, mydev, 1);
	if ( result < 0 )
	{
    		printk(KERN_WARNING "Failed to register the cdev structure");
    		return result;
	}

	return 0;
}


/*********************************************************************
**	Called when this driver module is unloaded
*********************************************************************/
static void __exit chardrv_out(void)
{
	printk(KERN_INFO "module chardrv being unloaded.\n");

	cdev_del(&my_cdev);
	unregister_chrdev_region(mydev, 1);

}

 

module_init(chardrv_in);
module_exit(chardrv_out);

MODULE_AUTHOR("Robert P. J. Day, http://crashcourse.ca");
MODULE_LICENSE("GPL");


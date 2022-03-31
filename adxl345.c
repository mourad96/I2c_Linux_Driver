#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/types.h>
#include "adxl345.h"
uint8_t reg = 0x32;
struct fifo_element { 
  // Un element de la FIFO, dans votre cas un échantillon de l'accéléromètre
  int16_t data_x;
  int16_t data_y;
  int16_t data_z;
};
struct adxl345_device{
    struct miscdevice msicdev;
    uint16_t dir;
    DECLARE_KFIFO(samples_fifo, struct fifo_element, 64);
    wait_queue_head_t queue;
    //DECLARE_WAIT_QUEUE_HEAD(queue);
};

short combine(unsigned char msb, unsigned char lsb) {
    return (msb<<8u)|lsb;
}


static int adxl345_open(struct inode *device_file, struct file *instance) {
	printk("dev_nr - open was called!\n");
	return 0;
}

static int adxl345_close(struct inode *device_file, struct file *instance) {
	printk("dev_nr - close was called!\n");
	return 0;
}
static ssize_t adxl345_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    pr_info("Driver Read Function Called...!!!\n");
    int res ;
    int ro;
    int i;
    uint8_t data_x[6];
    int16_t data[3];
    struct miscdevice *m = file->private_data;
    struct adxl345_device *adxl345 = container_of(m,struct adxl345_device, msicdev);
    struct device *d = m->parent;
    struct i2c_client *client = container_of(d, struct i2c_client, dev);
    /*i2c_master_send(client,&reg,1);
    i2c_master_recv (client ,data_x, 6);
    for (i = 0; i < 6; i++){
        pr_info("this is from the driver%d \n",data_x[i]);
    }*/
    struct fifo_element el;
    //
    // Test si la FIFO est vide
    if(!kfifo_is_empty(&adxl345->samples_fifo)){
        int r = kfifo_get(&adxl345->samples_fifo, &el);
        if(r == 0){
            pr_info("no Data in FIFO \n");
            }else{
                switch (adxl345->dir)
                {
                case cmd_x:
                    data[0] = el.data_x;
                    res = copy_to_user(buf, data[0], 2);
                    ro = 2;
                    break;
                case cmd_y:
                    data[1] = el.data_y;
                    res = copy_to_user(buf, data[1], 2);
                    ro = 2;
                    break;
                case cmd_z:
                    data[2] = el.data_z;
                    res = copy_to_user(buf, data[2], 2);
                    ro = 2;
                    break;
                case cmd_xyz:
                    data[0] = el.data_x;
                    pr_info("this is from the driver x = %d \n",data[0]);
                    data[1] = el.data_y;
                    pr_info("this is from the driver y = %d \n",data[1]);
                    data[2] = el.data_z;
                    pr_info("this is from the driver z = %d \n",data[2]);
                    if(len < 6){
                        res = copy_to_user(buf,data,len);
                        return len;
                    }else{
                        res = copy_to_user(buf,data,6);
                        pr_err("line 79");
                        return 6;
                    }
                    break;
                }
            }
        
    }else{
        //wait
        pr_err("wait");
        wait_event (adxl345->queue, !kfifo_is_empty(&adxl345->samples_fifo));
    }
    

    return ro;
}
//
static long adxl345_ioctl(struct file *f, unsigned int cmd)
{
    struct miscdevice *m = f->private_data;
    struct adxl345_device *adxl345 = container_of(m, struct adxl345_device, msicdev);
    switch (cmd)
    {
    case cmd_x:
        reg = 0x32;
        adxl345->dir = cmd_x;
        break;
    case cmd_y:
        reg = 0x34;
        adxl345->dir = cmd_y;
        break;
    case cmd_z:
        reg = 0x36;
        adxl345->dir = cmd_z;
        break;
    case cmd_xyz:
        reg = 0x32;
        adxl345->dir = cmd_xyz;
        break;
    }
    return 0;
}
struct file_operations f = {
        .owner = THIS_MODULE,
        .read = adxl345_read,
        .unlocked_ioctl = (long)adxl345_ioctl,
        .open = adxl345_open,
        .release = adxl345_close
    };

 //top half   
static irqreturn_t i2c_irq_handler(int irq, void *dev_id) 
{
  
  pr_info("Interrupt(IRQ Handler)\n");

  return IRQ_WAKE_THREAD;
}

//fonction bottom half
static irqreturn_t i2c_interrupt_adxl345_int(int irq, void *dev_id) 
{
    struct adxl345_device *adxl345 = dev_id;
    //pr_info("Interrupt bottom half\n");
    
    int i;
    uint8_t data[6];
    uint8_t FIFO_STATUS = 0x39;
    uint8_t value ;
    struct device *d = adxl345->msicdev.parent;
    struct i2c_client *client = container_of(d, struct i2c_client, dev);
    
    i2c_master_send(client,&FIFO_STATUS,1);
    i2c_master_recv ( client ,&value, 1);
    value &= 0x3f; //recuperer seuleument "Entries"
    //pr_info("le nombre d’échantillons disponibles dans la FIFO de l’accéléromètre est = %d \n",value);
    //get data and store it in FIFO
    struct fifo_element el;
    for ( i = 0; i < value+1; i++)
    {
        i2c_master_send(client,&reg,1);
        i2c_master_recv (client ,data, 6);
        el.data_x = combine(data[1],data[0]);
        // pr_info("value of data_x = %d \n",el.data_x);
        el.data_y = combine(data[3],data[2]);
       // pr_info("value of data_y = %d \n",el.data_y);
        el.data_z = combine(data[5],data[4]);
        //pr_info("value of data_z = %d \n",el.data_z);
        kfifo_put(&adxl345->samples_fifo, el);
    }
    /*i2c_master_send(client,&FIFO_STATUS,1);
    i2c_master_recv ( client ,&value, 1);
    value &= 0x3f; //recuperer seuleument "Entries"
    pr_info("le nombre d’échantillons disponibles dans la FIFO de l’accéléromètre est = %d \n",value);*/
    //wake up
    wake_up_interruptible(&adxl345->queue);
  return IRQ_HANDLED;
}


static int foo_probe(struct i2c_client *client,
                     const struct i2c_device_id *id)
{
    pr_info("adxl345 est détecté!\n");

    //config of adxl345 reg
    uint8_t reg_adxl = 0;
    uint8_t BW_RATE[2] = {0x2c,0b00001010};
    uint8_t INT_ENABLE[2] = {0x2E,0x02};//watermark enable
    uint8_t DATA_FORMAT = 0x31;
    uint8_t FIFO_CTL[] = {0x38,0x9F};//mode stream
    uint8_t POWER_CTL[] = {0x2D,0x08};
    uint8_t value ; 
    //Deuxième étape
    i2c_master_send(client,&reg_adxl,1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre DEVID est = %x \n",value);
    //Troisième étape
    i2c_master_send(client,BW_RATE,2);
    i2c_master_send(client,&BW_RATE[0],1);
    i2c_master_recv ( client ,&value, 1);
    i2c_master_send(client,INT_ENABLE,2);
    i2c_master_send(client,&INT_ENABLE[0],1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre INT_ENABLE est = %x \n",value);

    i2c_master_send(client,&DATA_FORMAT,1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre DATA_FORMAT est = %x \n",value);

    i2c_master_send(client,FIFO_CTL,2);
    i2c_master_send(client,&FIFO_CTL[0],1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre FIFO_CTL est = %x \n",value);

    i2c_master_send(client,POWER_CTL,2);
    i2c_master_send(client,&POWER_CTL[0],1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre POWER_CTL est = %x \n",value);
    
    ///////////////////////////
    static int x = 0; 
    struct adxl345_device *adxl345 = kzalloc (sizeof(struct adxl345_device), GFP_KERNEL);
    adxl345->msicdev.parent = &client->dev;
    adxl345->msicdev.minor = MISC_DYNAMIC_MINOR;
    adxl345->msicdev.name = kasprintf(GFP_KERNEL, "adxl345-%d",x++);
    adxl345->msicdev.fops = &f;
    i2c_set_clientdata(client, adxl345);
    misc_register(&adxl345->msicdev);
    INIT_KFIFO(adxl345->samples_fifo);
    ////
    if(devm_request_threaded_irq (&client->dev, client->irq, NULL ,i2c_interrupt_adxl345_int, IRQF_ONESHOT ,"adxl345_device", adxl345))
    {
        pr_err("my_device: cannot register IRQ ");
    }
    init_waitqueue_head(&adxl345->queue);
    return 0;
}

static int foo_remove(struct i2c_client *client)
{
    /*uint8_t POWER_CTL[] = {0x2D,0};
    uint8_t value ;
    i2c_master_send(client,POWER_CTL,2);
    i2c_master_send(client,&POWER_CTL[0],1);
    i2c_master_recv ( client ,&value, 1);
    pr_info("valeur du registre POWER_CTL est = %x \n",value);*/
    pr_info("adxl345 est retiré!\n");
    struct adxl345_device *adxl345 = i2c_get_clientdata(client);
    free_irq(client->irq,adxl345);
    kfree(adxl345);
    misc_deregister(&adxl345->msicdev);
     pr_err("line 207");
    return 0 ;
}

/* La liste suivante permet l'association entre un périphérique et son
   pilote dans le cas d'une initialisation statique sans utilisation de
   device tree.

   Chaque entrée contient une chaîne de caractère utilisée pour
   faire l'association et un entier qui peut être utilisé par le
   pilote pour effectuer des traitements différents en fonction
   du périphérique physique détecté (cas d'un pilote pouvant gérer
   différents modèles de périphérique).
*/
static struct i2c_device_id foo_idtable[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, foo_idtable);

#ifdef CONFIG_OF
/* Si le support des device trees est disponible, la liste suivante
   permet de faire l'association à l'aide du device tree.

   Chaque entrée contient une structure de type of_device_id. Le champ
   compatible est une chaîne qui est utilisée pour faire l'association
   avec les champs compatible dans le device tree. Le champ data est
   un pointeur void* qui peut être utilisé par le pilote pour
   effectuer des traitements différents en fonction du périphérique
   physique détecté.
*/
static const struct of_device_id foo_of_match[] = {
    { .compatible = "Analog Devices,adxl345",
      .data = NULL },
    {}
};

MODULE_DEVICE_TABLE(of, foo_of_match);
#endif

static struct i2c_driver foo_driver = {
    .driver = {
        /* Le champ name doit correspondre au nom du module
           et ne doit pas contenir d'espace */
        .name   = "adxl345",
        .of_match_table = of_match_ptr(foo_of_match),
    },

    .id_table       = foo_idtable,
    .probe          = foo_probe,
    .remove         = foo_remove,
};

module_i2c_driver(foo_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Foo driver");
MODULE_AUTHOR("Me");

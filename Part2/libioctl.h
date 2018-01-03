#include <linux/ioctl.h>

#define IOC_MAGIC 'k'
#define FLASHGETS _IOR(IOC_MAGIC, 0, unsigned long) 
#define FLASHSETP _IOW(IOC_MAGIC, 1, unsigned long)
#define FLASHGETP _IOR(IOC_MAGIC, 2, unsigned long)
#define FLASHERASE _IOW(IOC_MAGIC, 3, unsigned long) 



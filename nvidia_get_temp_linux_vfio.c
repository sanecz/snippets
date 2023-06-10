// Offsets might work for GP100+/GA104 but not on Tesla afaik
// but sensor is shadowed on ga104 you might need to fix the code
/*
  Retreive GPU Core temp on linux when using vfio-pci
  Works only when container is not used
 */
#include <linux/vfio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

int main() {
  int container, group, device, i;
 struct vfio_group_status group_status =
   { .argsz = sizeof(group_status) };
 struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
 struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
 struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

 /* Create a new container */
 container = open("/dev/vfio/vfio", O_RDWR);

 if (ioctl(container, VFIO_GET_API_VERSION) != VFIO_API_VERSION)
   return -1;
 /* Unknown API version */

 if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU))
   return -1;
 /* Doesn't support the IOMMU driver we want. */

 /* Open the group */
 group = open("/dev/vfio/45", O_RDONLY);
 if (group < 0)
   return -1;

 /* Test the group is viable and available */
 ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);

 if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))
   /* Group is not viable (ie, not all devices bound for vfio) */
   return -1;

 /* Add the group to the container */
 ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);

 /* Enable the IOMMU model we want */
 ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

 /* Get addition IOMMU info */
 ioctl(container, VFIO_IOMMU_GET_INFO, &iommu_info);

 /* Allocate some space and setup a DMA mapping */
 dma_map.vaddr = mmap(0, 1024 * 1024, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
 dma_map.size = 1024 * 1024;
 dma_map.iova = 0; /* 1MB starting at 0x0 from device view */
 dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

 ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map);

 /* Get a file descriptor for the device */
 device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, "0000:02:00.0");

 /* Test and setup the device */
 ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);

 /* Working only on BAR 0 */
 struct vfio_region_info regs = {
   .argsz = sizeof(struct vfio_region_info),
   .index  = 0
 };
 
 ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &regs);
 
 uint8_t* ptr = mmap(0, regs.size, PROT_READ, MAP_SHARED, device, 0);

 /* Stolen from you know where ;) */
 uint32_t tsensor = *(uint32_t*)(ptr + 0x020460);
 uint32_t inttemp = (tsensor & 0x0001fff8);
 
 if (tsensor & 0x40000000)
   printf("shadowed sensor\n");

 if (tsensor & 0x20000000)
   printf("temp %d\n", inttemp >> 8);

 /* Gratuitous device reset and go... */
 ioctl(device, VFIO_DEVICE_RESET);
 munmap(ptr, regs.size);

 return 0;
 
}

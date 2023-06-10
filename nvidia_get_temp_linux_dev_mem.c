// Offsets works for GA104
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pci/pci.h>


struct device
{
    uint32_t      bar0;
    unsigned long size;
    uint32_t      domain;
    const char    *bus;
};


int main(int argc, char **argv)
{
    struct device devices[32], *device;
    int fd;
    size_t nbgpus = 0, i = 0;
    void *map_base;
    uint32_t temp, tsensor;


    struct pci_access *pci;
    struct pci_dev *dev;

    pci = pci_alloc();
    pci_init(pci);
    pci_scan_bus(pci);

    for (dev = pci->devices; dev; dev = dev->next) {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES);
        if (dev->base_addr[0] != 0 && dev->device_id == 0x2232) {
            devices[nbgpus].size = dev->size[0];
            devices[nbgpus].bar0 = (dev->base_addr[0] & 0xFFFFFFFF);
            devices[nbgpus].domain = dev->domain;
            devices[nbgpus].bus = dev->bus;
            nbgpus++;
        }
    }


    fd = open("/dev/mem", O_RDONLY | O_SYNC);
    printf("DEV  : ");
    for (i = 0; i < nbgpus; i++) {
      device = &devices[i];
      printf("%04x:%02x  ", device->domain, device->bus);
    }
    printf("\n");
    while (1)
    {
        printf("VRAM : ");
        for (i = 0; i < nbgpus; i++) {

            device = &devices[i];
            map_base = mmap(0, device->size, PROT_READ, MAP_SHARED, fd, device->bar0);
            tsensor = *((uint32_t *) (map_base + 0xe2a8));
            temp = ((tsensor & 0x00000fff) / 0x20);
            printf("  %3u°c |", temp);
            munmap(map_base, device->size);
        }
        printf("\nGPU  : ");
        for (size_t i = 0; i < nbgpus; i++) {
            struct device *device = &devices[i];
            map_base = mmap(0, device->size, PROT_READ, MAP_SHARED, fd, device->bar0);
            tsensor = *(uint32_t*)(map_base + 0x020460);
            temp = (tsensor & 0x0001fff8) >> 8;
            printf("  %3u°c |", temp);
            munmap(map_base, device->size);
        }
        printf("\n");
        printf("\033[A\033[A");
        fflush(stdout);
        sleep(1);
    }
    pci_cleanup(pci);

    return 0;
}


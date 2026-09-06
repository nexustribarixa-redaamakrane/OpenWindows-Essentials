# OpenWindows Driver Architecture

## Driver Types
- **Bus Drivers** (pcibridge, i2c, spi)
- **Storage Drivers** (ahci, nvmedrv, atapio, floppy, cdrom)
- **Network Drivers** (e1000, rtl8139, ipstack, tcpcore, udpcore)
- **Input Drivers** (ps2hid, mousedrv, kbdlayout, i8042)
- **Display Drivers** (vgapci, vbedrv, fbdev, virtio_gpu)
- **Audio Drivers** (ac97, hdaudio)
- **System Drivers** (acpitbl, ioapic, lapic, hpet, smbios, cmos, rtc)
- **Virtual Drivers** (virtio_blk, virtio_net, ramdisk, loopback, devnull)
- **Security Drivers** (pfwall, entropy, watchdog)

## Driver Lifecycle
1. owc_entry_init() - Initialize driver
2. owc_entry_start() - Begin operation
3. owc_entry_stop() - Cease operation
4. owc_entry_cleanup() - Free resources


#!/usr/bin/env pwsh
# batch_create_ow_files.ps1 - Generates a large batch of OpenWindows components
# Run from OpenWindows-Essentials root

$Root = "c:\Users\KARIMABENDA\Documents\OpenWindows-Essentials"

# ─── Helper: write C file ──────────────────────────────────────
function Write-CFile($Path, $Content) {
    $dir = Split-Path $Path -Parent
    if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    Set-Content -Path $Path -Value $Content -Encoding UTF8
}

# Common header
$FreestandingIncludes = @"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
"@

# ═══════════════════════════════════════════════════════════════
# NEW DRIVERS (.owc)
# ═══════════════════════════════════════════════════════════════

$NewDrivers = @(
    @{ Name="acpitbl";   Desc="ACPI Table Parser";         Funcs=@("acpitbl_init","acpitbl_find_table","acpitbl_validate_rsdp") }
    @{ Name="ehci";      Desc="Enhanced Host Controller Interface (USB 2.0)"; Funcs=@("ehci_init","ehci_reset","ehci_poll_port","ehci_submit_urb") }
    @{ Name="xhci";      Desc="Extensible Host Controller Interface (USB 3.x)"; Funcs=@("xhci_init","xhci_reset","xhci_poll_port","xhci_submit_trb") }
    @{ Name="ahci";      Desc="Advanced Host Controller Interface (SATA)"; Funcs=@("ahci_init","ahci_probe_ports","ahci_read_sectors","ahci_write_sectors") }
    @{ Name="nvmedrv";   Desc="NVMe Storage Controller";   Funcs=@("nvme_init","nvme_identify","nvme_read","nvme_write","nvme_flush") }
    @{ Name="e1000";     Desc="Intel e1000 NIC Driver";     Funcs=@("e1000_init","e1000_send","e1000_recv","e1000_get_mac") }
    @{ Name="rtl8139";   Desc="Realtek RTL8139 NIC Driver"; Funcs=@("rtl8139_init","rtl8139_send","rtl8139_recv","rtl8139_reset") }
    @{ Name="ac97";      Desc="AC97 Audio Codec Driver";    Funcs=@("ac97_init","ac97_set_volume","ac97_play_buffer","ac97_stop") }
    @{ Name="hdaudio";   Desc="Intel HD Audio Driver";      Funcs=@("hda_init","hda_reset","hda_play_stream","hda_stop_stream","hda_set_volume") }
    @{ Name="i8042";     Desc="i8042 PS/2 Controller";      Funcs=@("i8042_init","i8042_send_cmd","i8042_read_data","i8042_flush") }
    @{ Name="pcibridge"; Desc="PCI Bus Bridge Enumerator";  Funcs=@("pcib_init","pcib_enumerate","pcib_read_config","pcib_write_config") }
    @{ Name="ioapic";    Desc="I/O APIC Driver";            Funcs=@("ioapic_init","ioapic_set_irq","ioapic_mask","ioapic_unmask","ioapic_eoi") }
    @{ Name="lapic";     Desc="Local APIC Driver";          Funcs=@("lapic_init","lapic_send_ipi","lapic_eoi","lapic_timer_set") }
    @{ Name="hpet";      Desc="HPET Timer Driver";          Funcs=@("hpet_init","hpet_read_counter","hpet_set_comparator","hpet_enable") }
    @{ Name="smbios";    Desc="SMBIOS Table Parser";        Funcs=@("smbios_init","smbios_find_table","smbios_get_string","smbios_version") }
    @{ Name="vbedrv";    Desc="VESA BIOS Extensions Driver"; Funcs=@("vbe_init","vbe_set_mode","vbe_get_framebuffer","vbe_get_info") }
    @{ Name="fbdev";     Desc="Framebuffer Device Driver";  Funcs=@("fbdev_init","fbdev_putpixel","fbdev_fill_rect","fbdev_blit","fbdev_clear") }
    @{ Name="ttydrv";    Desc="TTY Console Driver";         Funcs=@("tty_init","tty_write_char","tty_write_string","tty_clear","tty_scroll") }
    @{ Name="dmactl";    Desc="DMA Controller Driver";      Funcs=@("dma_init","dma_alloc_channel","dma_start_transfer","dma_wait") }
    @{ Name="atapio";    Desc="ATA PIO Mode Driver";        Funcs=@("ata_init","ata_identify","ata_read_sectors","ata_write_sectors") }
    @{ Name="floppy";    Desc="Floppy Disk Controller";     Funcs=@("floppy_init","floppy_read","floppy_write","floppy_seek","floppy_reset") }
    @{ Name="cmos";      Desc="CMOS/RTC Access Driver";     Funcs=@("cmos_init","cmos_read_byte","cmos_write_byte","cmos_get_time") }
    @{ Name="mousedrv";  Desc="Mouse Input Driver";         Funcs=@("mouse_init","mouse_poll","mouse_get_state","mouse_set_rate") }
    @{ Name="kbdlayout"; Desc="Keyboard Layout Manager";    Funcs=@("kbl_init","kbl_set_layout","kbl_scancode_to_char","kbl_get_current") }
    @{ Name="pcspkr";    Desc="PC Speaker Beep Driver";     Funcs=@("pcspkr_init","pcspkr_beep","pcspkr_set_freq","pcspkr_stop") }
    @{ Name="watchdog";  Desc="Hardware Watchdog Timer";    Funcs=@("wdt_init","wdt_kick","wdt_set_timeout","wdt_disable") }
    @{ Name="entropy";   Desc="Hardware Entropy Source";    Funcs=@("entropy_init","entropy_read","entropy_available","entropy_seed") }
    @{ Name="pwr";       Desc="Power Management Driver";    Funcs=@("pwr_init","pwr_shutdown","pwr_reboot","pwr_sleep","pwr_get_state") }
    @{ Name="thermal";   Desc="Thermal Monitor Driver";     Funcs=@("thermal_init","thermal_read_temp","thermal_set_threshold","thermal_get_zone") }
    @{ Name="battery";   Desc="Battery/ACPI Battery Driver"; Funcs=@("batt_init","batt_get_level","batt_get_status","batt_get_time_remain") }
    @{ Name="virtio_blk"; Desc="VirtIO Block Device";       Funcs=@("virtblk_init","virtblk_read","virtblk_write","virtblk_flush") }
    @{ Name="virtio_net"; Desc="VirtIO Network Device";     Funcs=@("virtnet_init","virtnet_send","virtnet_recv","virtnet_get_mac") }
    @{ Name="virtio_gpu"; Desc="VirtIO GPU Device";         Funcs=@("virtgpu_init","virtgpu_create_resource","virtgpu_transfer","virtgpu_flush") }
    @{ Name="owpipe";    Desc="Named Pipe IPC Driver";      Funcs=@("pipe_init","pipe_create","pipe_read","pipe_write","pipe_close") }
    @{ Name="devnull";   Desc="Null/Zero Device Driver";    Funcs=@("devnull_init","devnull_read","devnull_write") }
    @{ Name="loopback";  Desc="Loopback Block Device";      Funcs=@("loop_init","loop_attach","loop_detach","loop_read","loop_write") }
    @{ Name="partmgr";   Desc="Partition Manager";          Funcs=@("partmgr_init","partmgr_scan","partmgr_read_mbr","partmgr_read_gpt") }
    @{ Name="cdrom";     Desc="ATAPI CD-ROM Driver";        Funcs=@("cdrom_init","cdrom_read_sector","cdrom_eject","cdrom_capacity") }
    @{ Name="i2c";       Desc="I2C Bus Controller";         Funcs=@("i2c_init","i2c_read","i2c_write","i2c_scan_bus") }
    @{ Name="spi";       Desc="SPI Bus Controller";         Funcs=@("spi_init","spi_transfer","spi_set_mode","spi_set_speed") }
    @{ Name="gpio";      Desc="GPIO Controller";            Funcs=@("gpio_init","gpio_set_pin","gpio_get_pin","gpio_set_direction") }
    @{ Name="pfwall";    Desc="Packet Filter Firewall";     Funcs=@("pfw_init","pfw_add_rule","pfw_remove_rule","pfw_filter_packet","pfw_flush_rules") }
    @{ Name="ipstack";   Desc="Minimal IP Stack";           Funcs=@("ip_init","ip_send","ip_recv","ip_route","ip_arp_resolve") }
    @{ Name="tcpcore";   Desc="TCP Transport Core";         Funcs=@("tcp_init","tcp_connect","tcp_listen","tcp_send","tcp_recv","tcp_close") }
    @{ Name="udpcore";   Desc="UDP Transport Core";         Funcs=@("udp_init","udp_bind","udp_send","udp_recv","udp_close") }
    @{ Name="dhcpcli";   Desc="DHCP Client";                Funcs=@("dhcp_init","dhcp_discover","dhcp_request","dhcp_release") }
    @{ Name="dnscli";    Desc="DNS Resolver";               Funcs=@("dns_init","dns_resolve","dns_cache_flush","dns_set_server") }
)

foreach ($drv in $NewDrivers) {
    $name = $drv.Name
    $desc = $drv.Desc
    $funcs = $drv.Funcs
    $upper = $name.ToUpper()
    $dir = "$Root\Drivers\$name"

    # Header
    $hContent = @"
/*
 * $name.h - OpenWindows $desc (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ${upper}_H
#define ${upper}_H

$FreestandingIncludes

"@
    foreach ($f in $funcs) {
        $hContent += "void ${f}(void);`n"
    }
    $hContent += "`n#endif /* ${upper}_H */`n"
    Write-CFile "$dir\$name.h" $hContent

    # Source
    $cContent = @"
/*
 * $name.c - OpenWindows $desc (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "$name.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ${name}_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */

"@
    foreach ($f in $funcs) {
        $cContent += @"
void ${f}(void)
{
    if (!${name}_initialized) { return; }
    /* TODO: implement $f */
    (void)0;
}

"@
    }
    Write-CFile "$dir\$name.c" $cContent
    Write-Host "  [DRV] $name ($($funcs.Count) functions)"
}

# ═══════════════════════════════════════════════════════════════
# NEW DLLs (.owd)
# ═══════════════════════════════════════════════════════════════

$NewDLLs = @(
    @{ Name="owlog64";    Desc="Kernel Logging Library";       Funcs=@("owlog_init","owlog_write","owlog_flush","owlog_set_level","owlog_get_buffer") }
    @{ Name="owdebug64";  Desc="Kernel Debug Support Library"; Funcs=@("owdbg_init","owdbg_breakpoint","owdbg_print","owdbg_dump_regs","owdbg_stack_trace") }
    @{ Name="owtime64";   Desc="Time Management Library";      Funcs=@("owtime_init","owtime_get_ticks","owtime_sleep_ms","owtime_get_uptime","owtime_set_epoch") }
    @{ Name="owio64";     Desc="Port I/O Abstraction Library"; Funcs=@("owio_inb","owio_outb","owio_inw","owio_outw","owio_ind","owio_outd","owio_wait") }
    @{ Name="owirq64";    Desc="IRQ Management Library";       Funcs=@("owirq_init","owirq_register","owirq_unregister","owirq_enable","owirq_disable","owirq_acknowledge") }
    @{ Name="owsync64";   Desc="Synchronization Primitives";   Funcs=@("owsync_spinlock_init","owsync_spinlock_acquire","owsync_spinlock_release","owsync_mutex_init","owsync_mutex_lock","owsync_mutex_unlock","owsync_barrier") }
    @{ Name="owheap64";   Desc="Managed Heap Allocator";       Funcs=@("owheap_init","owheap_alloc","owheap_free","owheap_realloc","owheap_stats") }
    @{ Name="owstring64"; Desc="String Operations Library";    Funcs=@("owstr_len","owstr_copy","owstr_compare","owstr_concat","owstr_find","owstr_to_upper","owstr_to_lower","owstr_format") }
    @{ Name="owmath64";   Desc="Fixed-Point Math Library";     Funcs=@("owmath_abs","owmath_min","owmath_max","owmath_clamp","owmath_sqrt_approx","owmath_div_round") }
    @{ Name="owacl64";    Desc="Access Control List Library";  Funcs=@("owacl_init","owacl_check","owacl_grant","owacl_revoke","owacl_enumerate") }
    @{ Name="owfs64";     Desc="Filesystem Abstraction Layer"; Funcs=@("owfs_init","owfs_mount","owfs_unmount","owfs_open","owfs_close","owfs_read","owfs_write","owfs_stat") }
    @{ Name="owelf64";    Desc="OWX Executable Loader";        Funcs=@("owelf_load","owelf_validate","owelf_relocate","owelf_resolve_imports","owelf_get_entry") }
    @{ Name="owdev64";    Desc="Device Manager Library";       Funcs=@("owdev_init","owdev_register","owdev_unregister","owdev_find","owdev_enumerate","owdev_get_info") }
    @{ Name="owpci64";    Desc="PCI Configuration Library";    Funcs=@("owpci_read8","owpci_read16","owpci_read32","owpci_write8","owpci_write16","owpci_write32","owpci_find_device") }
    @{ Name="owdisk64";   Desc="Disk I/O Abstraction";         Funcs=@("owdisk_init","owdisk_read","owdisk_write","owdisk_flush","owdisk_get_info","owdisk_enumerate") }
    @{ Name="ownet64";    Desc="Network Stack Library";        Funcs=@("ownet_init","ownet_socket","ownet_bind","ownet_connect","ownet_send","ownet_recv","ownet_close") }
    @{ Name="owgfx64";    Desc="Graphics Primitives Library";  Funcs=@("owgfx_init","owgfx_draw_line","owgfx_draw_rect","owgfx_draw_circle","owgfx_fill_rect","owgfx_blit_bitmap") }
    @{ Name="owfont64";   Desc="Font Rendering Engine";        Funcs=@("owfont_init","owfont_load","owfont_render_glyph","owfont_measure_text","owfont_set_size") }
    @{ Name="owclip64";   Desc="Clipboard Manager";            Funcs=@("owclip_init","owclip_copy","owclip_paste","owclip_clear","owclip_get_format") }
    @{ Name="owtheme64";  Desc="Theme Engine Library";         Funcs=@("owtheme_init","owtheme_load","owtheme_apply","owtheme_get_color","owtheme_get_font") }
    @{ Name="owdlg64";    Desc="Dialog/Message Box Library";   Funcs=@("owdlg_init","owdlg_message","owdlg_confirm","owdlg_input","owdlg_file_open","owdlg_file_save") }
    @{ Name="owmenu64";   Desc="Menu System Library";          Funcs=@("owmenu_init","owmenu_create","owmenu_add_item","owmenu_show","owmenu_destroy","owmenu_get_selection") }
    @{ Name="owicon64";   Desc="Icon/Bitmap Resource Library"; Funcs=@("owicon_init","owicon_load","owicon_draw","owicon_get_size","owicon_create") }
    @{ Name="owdnd64";    Desc="Drag and Drop Library";        Funcs=@("owdnd_init","owdnd_begin_drag","owdnd_drop","owdnd_register_target","owdnd_get_data") }
    @{ Name="ownotif64";  Desc="Notification Service Library"; Funcs=@("ownotif_init","ownotif_send","ownotif_dismiss","ownotif_register","ownotif_get_pending") }
    @{ Name="owaudio64";  Desc="Audio Mixer Library";          Funcs=@("owaudio_init","owaudio_play","owaudio_stop","owaudio_set_volume","owaudio_get_devices") }
    @{ Name="owprint64";  Desc="Print Spooler Library";        Funcs=@("owprint_init","owprint_submit","owprint_cancel","owprint_get_queue","owprint_enum_printers") }
    @{ Name="owlocale64"; Desc="Locale/i18n Library";          Funcs=@("owlocale_init","owlocale_set","owlocale_get","owlocale_format_number","owlocale_format_date") }
    @{ Name="owreg64";    Desc="Registry Access Library";      Funcs=@("owreg_open","owreg_close","owreg_read","owreg_write","owreg_delete","owreg_enum_keys") }
    @{ Name="owxml64";    Desc="Minimal XML Parser";           Funcs=@("owxml_init","owxml_parse","owxml_get_root","owxml_find_element","owxml_get_attr","owxml_free") }
    @{ Name="owjson64";   Desc="Minimal JSON Parser";          Funcs=@("owjson_init","owjson_parse","owjson_get_string","owjson_get_number","owjson_get_array","owjson_free") }
    @{ Name="owzip64";    Desc="Data Compression Library";     Funcs=@("owzip_compress","owzip_decompress","owzip_crc32","owzip_adler32") }
    @{ Name="owhash64";   Desc="Hash Functions Library";       Funcs=@("owhash_sha256_init","owhash_sha256_update","owhash_sha256_final","owhash_md5","owhash_crc32c") }
    @{ Name="owtls64";    Desc="TLS/Crypto Transport Layer";   Funcs=@("owtls_init","owtls_handshake","owtls_send","owtls_recv","owtls_close") }
    @{ Name="owsock64";   Desc="Socket Abstraction Library";   Funcs=@("owsock_create","owsock_bind","owsock_listen","owsock_accept","owsock_connect","owsock_send","owsock_recv","owsock_close") }
    @{ Name="owprocfs64"; Desc="Process Filesystem Library";   Funcs=@("owprocfs_init","owprocfs_read_pid","owprocfs_list","owprocfs_get_stat","owprocfs_get_mem") }
    @{ Name="owsysfs64";  Desc="System Filesystem Library";    Funcs=@("owsysfs_init","owsysfs_read","owsysfs_write","owsysfs_enumerate","owsysfs_get_attr") }
    @{ Name="owevent64";  Desc="Event Bus Library";            Funcs=@("owevent_init","owevent_subscribe","owevent_unsubscribe","owevent_publish","owevent_poll") }
    @{ Name="owmsg64";    Desc="Message Queue Library";        Funcs=@("owmsg_init","owmsg_send","owmsg_recv","owmsg_peek","owmsg_flush") }
    @{ Name="owsignal64"; Desc="Signal Handling Library";      Funcs=@("owsig_init","owsig_register","owsig_raise","owsig_mask","owsig_pending") }
    @{ Name="owthread64"; Desc="Thread Management Library";    Funcs=@("owthread_create","owthread_join","owthread_detach","owthread_yield","owthread_exit","owthread_self") }
    @{ Name="owfifo64";   Desc="FIFO Ring Buffer Library";     Funcs=@("owfifo_init","owfifo_push","owfifo_pop","owfifo_peek","owfifo_count","owfifo_is_empty") }
    @{ Name="owpool64";   Desc="Memory Pool Allocator";        Funcs=@("owpool_init","owpool_alloc","owpool_free","owpool_reset","owpool_stats") }
    @{ Name="owbitmap64"; Desc="Bitmap Allocator";             Funcs=@("owbmp_init","owbmp_alloc","owbmp_free","owbmp_test","owbmp_find_first_free") }
    @{ Name="owcache64";  Desc="Block Cache Library";          Funcs=@("owcache_init","owcache_lookup","owcache_insert","owcache_evict","owcache_flush") }
    @{ Name="owslab64";   Desc="Slab Allocator";               Funcs=@("owslab_init","owslab_alloc","owslab_free","owslab_create_cache","owslab_destroy_cache") }
)

foreach ($dll in $NewDLLs) {
    $name = $dll.Name
    $desc = $dll.Desc
    $funcs = $dll.Funcs
    $upper = $name.ToUpper() -replace '-','_'
    $dir = "$Root\DLL\$name"

    # Header
    $hContent = @"
/*
 * $name.h - OpenWindows $desc (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ${upper}_H
#define ${upper}_H

$FreestandingIncludes

"@
    foreach ($f in $funcs) {
        $hContent += "void ${f}(void);`n"
    }
    $hContent += "`n#endif /* ${upper}_H */`n"
    Write-CFile "$dir\$name.h" $hContent

    # Source
    $cContent = @"
/*
 * $name.c - OpenWindows $desc (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "$name.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ${name}_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */

"@
    foreach ($f in $funcs) {
        $cContent += @"
void ${f}(void)
{
    (void)${name}_ready;
    /* TODO: implement $f */
}

"@
    }
    Write-CFile "$dir\$name.c" $cContent
    Write-Host "  [DLL] $name ($($funcs.Count) functions)"
}

# ═══════════════════════════════════════════════════════════════
# NEW SOFTWARE / EXECUTABLES (.owx)
# ═══════════════════════════════════════════════════════════════

$NewApps = @(
    @{ Name="owcp";       Desc="File Copy Utility" }
    @{ Name="owmv";       Desc="File Move/Rename Utility" }
    @{ Name="owrm";       Desc="File Delete Utility" }
    @{ Name="owls";       Desc="Directory Listing Utility" }
    @{ Name="owcat";      Desc="File Concatenation/View Utility" }
    @{ Name="owmkdir";    Desc="Directory Creation Utility" }
    @{ Name="owrmdir";    Desc="Directory Removal Utility" }
    @{ Name="owfind";     Desc="File Search Utility" }
    @{ Name="owgrep";     Desc="Pattern Search Utility" }
    @{ Name="owhead";     Desc="File Head Viewer" }
    @{ Name="owtail";     Desc="File Tail Viewer" }
    @{ Name="owwc";       Desc="Word/Line/Byte Counter" }
    @{ Name="owsort";     Desc="Line Sort Utility" }
    @{ Name="owhexdump";  Desc="Hex Dump Utility" }
    @{ Name="owchmod";    Desc="Permission Change Utility" }
    @{ Name="owchown";    Desc="Ownership Change Utility" }
    @{ Name="owstat";     Desc="File Status Utility" }
    @{ Name="owdf";       Desc="Disk Free Space Utility" }
    @{ Name="owdu";       Desc="Disk Usage Utility" }
    @{ Name="owps";       Desc="Process Status Utility" }
    @{ Name="owtop";      Desc="System Monitor Utility" }
    @{ Name="ownetstat";  Desc="Network Status Utility" }
    @{ Name="owping";     Desc="Network Ping Utility" }
    @{ Name="owroute";    Desc="Network Route Utility" }
    @{ Name="owifconfig"; Desc="Network Interface Config" }
    @{ Name="owmount";    Desc="Filesystem Mount Utility" }
    @{ Name="owumount";   Desc="Filesystem Unmount Utility" }
    @{ Name="owformat";   Desc="Disk Format Utility" }
    @{ Name="owfdisk";    Desc="Partition Editor Utility" }
    @{ Name="owsysinfo";  Desc="System Information Utility" }
    @{ Name="owdate";     Desc="Date/Time Display Utility" }
    @{ Name="owuptime";   Desc="System Uptime Utility" }
    @{ Name="owecho";     Desc="Text Echo Utility" }
    @{ Name="owsleep";    Desc="Sleep/Delay Utility" }
    @{ Name="owtrue";     Desc="Always-True Exit Utility" }
    @{ Name="owfalse";    Desc="Always-False Exit Utility" }
    @{ Name="owenv";      Desc="Environment Variable Viewer" }
    @{ Name="owset";      Desc="Environment Variable Setter" }
    @{ Name="owalias";    Desc="Command Alias Manager" }
    @{ Name="owhistory";  Desc="Command History Viewer" }
    @{ Name="owclear";    Desc="Console Clear Utility" }
    @{ Name="owreboot";   Desc="System Reboot Utility" }
    @{ Name="owhalt";     Desc="System Halt Utility" }
    @{ Name="owservice";  Desc="Service Manager CLI" }
    @{ Name="owuser";     Desc="User Account Manager" }
    @{ Name="owpasswd";   Desc="Password Change Utility" }
    @{ Name="owlogin";    Desc="Login Session Utility" }
    @{ Name="owlogout";   Desc="Logout Utility" }
    @{ Name="owsu";       Desc="Privilege Elevation Utility" }
    @{ Name="owtar";      Desc="Archive Utility" }
    @{ Name="owzip";      Desc="Compression Utility" }
    @{ Name="owunzip";    Desc="Decompression Utility" }
    @{ Name="owcalc";     Desc="Calculator Utility" }
    @{ Name="owcron";     Desc="Scheduled Task Manager" }
    @{ Name="owat";       Desc="One-time Task Scheduler" }
    @{ Name="owdmesg";    Desc="Kernel Message Buffer Viewer" }
    @{ Name="owlsmod";    Desc="Loaded Module Lister" }
    @{ Name="owinsmod";   Desc="Module Installer" }
    @{ Name="owrmmod";    Desc="Module Remover" }
    @{ Name="owlspci";    Desc="PCI Device Lister" }
    @{ Name="owlsblk";    Desc="Block Device Lister" }
    @{ Name="owlsdrv";    Desc="Driver Lister" }
    @{ Name="owlsdll";    Desc="DLL Lister" }
    @{ Name="owkextmgr";  Desc="Kernel Extension Manager" }
    @{ Name="owregctl";   Desc="Registry Control Utility" }
    @{ Name="owfwupd";    Desc="Firmware Update Utility" }
    @{ Name="owdiag";     Desc="System Diagnostics Tool" }
    @{ Name="owbench";    Desc="System Benchmark Tool" }
    @{ Name="owmemtest";  Desc="Memory Test Utility" }
    @{ Name="owdiskchk";  Desc="Disk Integrity Checker" }
    @{ Name="owfsck";     Desc="Filesystem Check/Repair" }
    @{ Name="owbackup";   Desc="Backup Utility" }
    @{ Name="owrestore";  Desc="Restore Utility" }
    @{ Name="owscreenshot"; Desc="Screenshot Capture Tool" }
    @{ Name="owtaskbar";  Desc="Taskbar Application" }
    @{ Name="owstartmenu"; Desc="Start Menu Application" }
    @{ Name="owterm";     Desc="Terminal Emulator" }
    @{ Name="owfm";       Desc="File Manager" }
    @{ Name="owimgview";  Desc="Image Viewer" }
    @{ Name="owmediaplayer"; Desc="Media Player" }
    @{ Name="ownotepad";  Desc="Simple Notepad" }
    @{ Name="owclock";    Desc="Clock Widget" }
    @{ Name="owcalcgui";  Desc="GUI Calculator" }
    @{ Name="owsettings"; Desc="System Settings GUI" }
    @{ Name="owabout";    Desc="About Dialog" }
    @{ Name="owpaint";    Desc="Simple Paint Application" }
    @{ Name="owinstaller"; Desc="Package Installer GUI" }
    @{ Name="owtextdiff"; Desc="Text Diff Utility" }
)

foreach ($app in $NewApps) {
    $name = $app.Name
    $desc = $app.Desc
    $upper = $name.ToUpper() -replace '-','_'
    $dir = "$Root\Software\$name"

    $cContent = @"
/*
 * $name.c - OpenWindows $desc (.owx)
 *
 * Native executable for the OpenWindows environment.
 * C99 freestanding. Zero dynamic heap allocation.
 */

$FreestandingIncludes

/* ── Constants ────────────────────────────────────────────────── */

#define ${upper}_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  ${name}_run(int argc, const char *const *argv);
static void ${name}_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        ${name}_usage();
        return 1;
    }
    return ${name}_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int ${name}_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    /* TODO: implement $desc */
    return 0;
}

static void ${name}_usage(void)
{
    /* TODO: print usage for $name */
    (void)0;
}
"@
    Write-CFile "$dir\$name.c" $cContent
    Write-Host "  [APP] $name"
}

# ═══════════════════════════════════════════════════════════════
# KERNEL SCRIPTS (.kscr - plaintext)
# ═══════════════════════════════════════════════════════════════

$Scripts = @(
    @{ Name="boot_init.kscr";       Content="# OpenWindows Boot Initialization Script`n# Executed by owinit during early boot`n`nload_driver sio`nload_driver ps2hid`nload_driver vgapci`nload_driver pitapic`nload_driver rtc`nload_driver ramdisk`nload_driver pcibridge`nload_driver ioapic`nload_driver lapic`nmount ramdisk0 /tmp`ninit_console tty0`nset_keymap us`nstart_service banhammer`nstart_service sentinel`nlog `"Boot init complete`"`n" }
    @{ Name="net_init.kscr";        Content="# OpenWindows Network Initialization`n`nload_driver e1000`nload_driver rtl8139`nload_driver ipstack`nload_driver tcpcore`nload_driver udpcore`nload_driver pfwall`nstart_service dhcpcli`nstart_service dnscli`nlog `"Network stack initialized`"`n" }
    @{ Name="storage_init.kscr";    Content="# OpenWindows Storage Initialization`n`nload_driver ahci`nload_driver nvmedrv`nload_driver atapio`nload_driver partmgr`nload_driver owfsdrv`nscan_partitions`nmount owfs0 /`nmount owfs1 /home`nlog `"Storage initialized`"`n" }
    @{ Name="usb_init.kscr";        Content="# OpenWindows USB Initialization`n`nload_driver ehci`nload_driver xhci`nscan_usb_devices`nlog `"USB subsystem ready`"`n" }
    @{ Name="audio_init.kscr";      Content="# OpenWindows Audio Initialization`n`nload_driver ac97`nload_driver hdaudio`nset_volume 75`nlog `"Audio subsystem ready`"`n" }
    @{ Name="display_init.kscr";    Content="# OpenWindows Display Initialization`n`nload_driver vbedrv`nload_driver fbdev`nset_mode 1920x1080x32`nload_dll owgfx64`nload_dll owfont64`nlog `"Display initialized`"`n" }
    @{ Name="security_init.kscr";   Content="# OpenWindows Security Initialization`n`nload_dll owacl64`nload_dll owcrypt`nload_driver entropy`ninit_random_seed`nlog `"Security subsystem ready`"`n" }
    @{ Name="gui_startup.kscr";     Content="# OpenWindows GUI Startup`n`nload_dll owwm64`nload_dll owtheme64`nload_dll owicon64`nload_dll owmenu64`nload_dll owdlg64`nstart owwm`nstart owtaskbar`nstart owstartmenu`nstart owclock`nlog `"GUI environment started`"`n" }
    @{ Name="shutdown.kscr";        Content="# OpenWindows Shutdown Sequence`n`nflush_caches`nsync_filesystems`nstop_services`nunmount_all`nsave_registry`nlog `"Shutdown complete`"`npoweroff`n" }
    @{ Name="recovery.kscr";        Content="# OpenWindows Recovery Mode`n`nset_mode text`nload_driver sio`nload_driver atapio`nmount_readonly owfs0 /`nstart owsh --recovery`nlog `"Recovery shell started`"`n" }
    @{ Name="service_list.kscr";    Content="# OpenWindows Default Services`n# Format: service_name priority auto_start`n`nbanhammer 0 yes`nsentinel 1 yes`nowwm 5 yes`nowtaskbar 6 yes`nowcron 10 yes`ndhcpcli 15 yes`ndnscli 16 yes`nowprint64 20 no`n" }
    @{ Name="firewall_rules.kscr";  Content="# OpenWindows Default Firewall Rules`n# Format: action protocol src_port dst_port`n`nallow tcp * 80`nallow tcp * 443`nallow udp * 53`nallow icmp * *`ndeny tcp * 23`ndeny tcp * 445`nlog_dropped yes`n" }
)

$scriptDir = "$Root\Config\scripts"
foreach ($scr in $Scripts) {
    Write-CFile "$scriptDir\$($scr.Name)" $scr.Content
    Write-Host "  [KSCR] $($scr.Name)"
}

# ═══════════════════════════════════════════════════════════════
# CACHE FORMAT FILES (.kcache - hybrid binary/text headers)
# ═══════════════════════════════════════════════════════════════

$CacheHeaders = @(
    @{ Name="dns_cache";   Desc="DNS Resolution Cache" }
    @{ Name="font_cache";  Desc="Font Glyph Cache" }
    @{ Name="icon_cache";  Desc="Icon Bitmap Cache" }
    @{ Name="block_cache"; Desc="Disk Block Cache" }
    @{ Name="route_cache"; Desc="Network Route Cache" }
    @{ Name="pci_cache";   Desc="PCI Device Enumeration Cache" }
    @{ Name="theme_cache"; Desc="Theme Resource Cache" }
    @{ Name="acl_cache";   Desc="ACL Permission Cache" }
)

foreach ($cache in $CacheHeaders) {
    $name = $cache.Name
    $desc = $cache.Desc
    $upper = $name.ToUpper()
    $hContent = @"
/*
 * ${name}.h - OpenWindows $desc (.kcache)
 *
 * Binary cache format with text-parseable header section.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ${upper}_H
#define ${upper}_H

$FreestandingIncludes

#define ${upper}_MAGIC    0x4B434348u  /* 'KCCH' */
#define ${upper}_VERSION  1u
#define ${upper}_MAX_ENTRIES 4096u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t total_size;
    uint32_t checksum;
    uint8_t  reserved[12];
} ${name}_header_t;

typedef struct {
    uint32_t key_hash;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t ttl_seconds;
    uint64_t timestamp;
} ${name}_entry_t;

bool ${name}_init(void *buffer, size_t size);
bool ${name}_lookup(const void *buffer, uint32_t key, void *out, size_t *out_size);
bool ${name}_insert(void *buffer, uint32_t key, const void *data, size_t size, uint32_t ttl);
void ${name}_flush(void *buffer);
uint32_t ${name}_count(const void *buffer);

#endif /* ${upper}_H */
"@
    Write-CFile "$Root\Extensions\caches\${name}.h" $hContent
    Write-Host "  [CACHE] $name"
}

# ═══════════════════════════════════════════════════════════════
# BINARY FORMAT SPECS
# ═══════════════════════════════════════════════════════════════

$BinaryFormats = @(
    @{ Name="owimg";   Magic="0x4F574947"; Desc="OpenWindows Disk Image" }
    @{ Name="owpkg";   Magic="0x4F57504B"; Desc="OpenWindows Package Archive" }
    @{ Name="owsnap";  Magic="0x4F575350"; Desc="OpenWindows Snapshot/Checkpoint" }
    @{ Name="owdump";  Magic="0x4F574450"; Desc="OpenWindows Memory Dump" }
    @{ Name="owprof";  Magic="0x4F575046"; Desc="OpenWindows Profiler Data" }
    @{ Name="owfwup";  Magic="0x4F574655"; Desc="OpenWindows Firmware Update Package" }
    @{ Name="owtrace"; Magic="0x4F575452"; Desc="OpenWindows Execution Trace" }
    @{ Name="owcore";  Magic="0x4F574352"; Desc="OpenWindows Core Dump" }
)

foreach ($fmt in $BinaryFormats) {
    $name = $fmt.Name
    $desc = $fmt.Desc
    $magic = $fmt.Magic
    $upper = $name.ToUpper()
    $hContent = @"
/*
 * ${name}_format.h - $desc binary format specification
 *
 * C99 freestanding.
 */

#ifndef ${upper}_FORMAT_H
#define ${upper}_FORMAT_H

$FreestandingIncludes

#define ${upper}_MAGIC   ${magic}u
#define ${upper}_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} ${name}_header_t;

#endif /* ${upper}_FORMAT_H */
"@
    Write-CFile "$Root\Extensions\formats\${name}_format.h" $hContent
    Write-Host "  [FMT] $name"
}

# ═══════════════════════════════════════════════════════════════
# KERNEL TEXT FILES (.ktext)
# ═══════════════════════════════════════════════════════════════

$KTexts = @(
    @{ Name="motd.ktext";           Content="Welcome to OpenWindows.`nType 'owhelp' for assistance.`n" }
    @{ Name="version.ktext";       Content="OpenWindows Kernel v0.1.0-alpha`nBuild: freestanding-c99`nArch: x86_64`n" }
    @{ Name="license.ktext";       Content="OpenWindows Kernel`nCopyright (c) 2026 OpenWindows Project`nReleased under open-source license.`n" }
    @{ Name="panic_header.ktext";  Content="=== KERNEL PANIC ===`nAn unrecoverable error has occurred.`nPlease restart your system.`n" }
    @{ Name="boot_banner.ktext";   Content=" ___                 _    _ _           _`n/ _ \ _ __   ___ _ _| |  | (_)_ __   __| | _____      _____`n| | | | '_ \ / _ \ '_ \ |/\| | | '_ \ / _`` |/ _ \ \ /\ / / __/`n| |_| | |_) |  __/ | | \  /\  / | | | | (_| | (_) \ V  V /\__ \`n \___/| .__/ \___|_| |_|\/  \/|_|_| |_|\__,_|\___/ \_/\_/ |___/`n     |_|`n" }
    @{ Name="help_text.ktext";     Content="OpenWindows Built-in Commands:`n  owhelp    - Show this help`n  owls      - List directory contents`n  owcat     - Display file contents`n  owps      - List running processes`n  owsh      - Launch shell`n  owreboot  - Reboot system`n  owhalt    - Halt system`n  owdmesg   - Show kernel messages`n  owlsmod   - List loaded modules`n  owlspci   - List PCI devices`n" }
    @{ Name="safe_mode.ktext";     Content="OpenWindows Safe Mode`nMinimal drivers loaded. Network disabled.`nUse 'owdiag' to diagnose issues.`n" }
    @{ Name="first_boot.ktext";    Content="Welcome to OpenWindows - First Boot Setup`nPlease configure your system.`nRun 'owinit --setup' to begin.`n" }
    @{ Name="credits.ktext";       Content="OpenWindows Kernel`nA clean-slate non-POSIX NT-like operating system.`nDeveloped with passion and freestanding C99.`n" }
)

foreach ($kt in $KTexts) {
    Write-CFile "$Root\Config\ktexts\$($kt.Name)" $kt.Content
    Write-Host "  [KTEXT] $($kt.Name)"
}

# ═══════════════════════════════════════════════════════════════
# KERNEL EXTENSION SPECS (.kext)
# ═══════════════════════════════════════════════════════════════

$KExts = @(
    @{ Name="kext_crypto";   Desc="Cryptographic Extension" }
    @{ Name="kext_compress"; Desc="Compression Extension" }
    @{ Name="kext_audit";    Desc="Security Audit Extension" }
    @{ Name="kext_profile";  Desc="Performance Profiler Extension" }
    @{ Name="kext_trace";    Desc="Execution Tracer Extension" }
    @{ Name="kext_sandbox";  Desc="Process Sandbox Extension" }
    @{ Name="kext_quota";    Desc="Disk Quota Extension" }
    @{ Name="kext_snap";     Desc="Filesystem Snapshot Extension" }
    @{ Name="kext_mirror";   Desc="Disk Mirror/RAID Extension" }
    @{ Name="kext_encrypt";  Desc="Full Disk Encryption Extension" }
    @{ Name="kext_netmon";   Desc="Network Monitor Extension" }
    @{ Name="kext_hotplug";  Desc="Hot-plug Device Extension" }
)

foreach ($kx in $KExts) {
    $name = $kx.Name
    $desc = $kx.Desc
    $upper = $name.ToUpper()
    $hContent = @"
/*
 * ${name}.h - OpenWindows $desc
 *
 * Kernel extension module loaded at runtime via kextctl.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ${upper}_H
#define ${upper}_H

$FreestandingIncludes

#define ${upper}_VERSION 1u

typedef struct {
    const char *name;
    uint32_t    version;
    void      (*init)(void);
    void      (*cleanup)(void);
    bool      (*is_supported)(void);
} ${name}_ops_t;

void ${name}_register(const ${name}_ops_t *ops);
void ${name}_unregister(const char *name);
bool ${name}_is_loaded(void);

#endif /* ${upper}_H */
"@
    Write-CFile "$Root\Extensions\kext\${name}.h" $hContent

    $cContent = @"
/*
 * ${name}.c - OpenWindows $desc
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "${name}.h"

static bool ${name}_loaded = false;

void ${name}_register(const ${name}_ops_t *ops)
{
    if (!ops || !ops->init || !ops->is_supported) { return; }
    if (!ops->is_supported()) { return; }
    ops->init();
    ${name}_loaded = true;
}

void ${name}_unregister(const char *name)
{
    (void)name;
    ${name}_loaded = false;
}

bool ${name}_is_loaded(void)
{
    return ${name}_loaded;
}
"@
    Write-CFile "$Root\Extensions\kext\${name}.c" $cContent
    Write-Host "  [KEXT] $name"
}

# ═══════════════════════════════════════════════════════════════
# BANHAMMER DATA FILES
# ═══════════════════════════════════════════════════════════════

$BanHammerTexts = @"
# BANHammer Crash Texts
# One per line - chosen randomly when the kernel crashes
# Format: CATEGORY|TEXT

SARCASTIC|Well, that was unexpected. Just kidding, I totally saw that coming.
SARCASTIC|Congratulations! You found a bug. Here's your prize: a crash screen.
SARCASTIC|The kernel decided to take an unscheduled vacation. Sorry for the inconvenience.
SARCASTIC|On a scale of 1 to catastrophic, this is a solid "yes."
SARCASTIC|If you're reading this, the good news is your monitor works.
SARCASTIC|The system encountered a fatal error. The system also doesn't care about your feelings.
SARCASTIC|This crash was brought to you by: questionable life choices in the driver code.
SARCASTIC|Don't worry, your data is probably fine. Probably.
SARCASTIC|Have you tried turning it off and on again? Because that's literally your only option.
SARCASTIC|The kernel has left the building. Elvis style.
SARCASTIC|Error: Success. Just kidding. Error: Error.
SARCASTIC|Look on the bright side: at least it crashed gracefully. Well, semi-gracefully.
SARCASTIC|The kernel panicked. To be fair, so would you.
SARCASTIC|Task failed successfully... wait, no. Just failed.
SARCASTIC|Your system just blue-screened. Except it's not blue. It's worse.
INFORMATIONAL|A critical system error has occurred. Error code: {BAN_CODE}
INFORMATIONAL|The system has been halted to prevent data corruption. Error: {BAN_CODE}
INFORMATIONAL|An unrecoverable exception occurred at address {FAULT_ADDR}. Code: {BAN_CODE}
INFORMATIONAL|Kernel panic - not syncing: {BAN_REASON}
INFORMATIONAL|System halted. Please report error code {BAN_CODE} to your administrator.
INFORMATIONAL|A driver caused an unhandled exception. Driver: {DRIVER_NAME}. Code: {BAN_CODE}
INFORMATIONAL|Memory corruption detected at {FAULT_ADDR}. The system has been stopped.
INFORMATIONAL|Stack overflow in kernel mode. The system cannot continue.
INFORMATIONAL|Critical process {PROCESS_NAME} terminated unexpectedly. System halted.
INFORMATIONAL|Filesystem integrity check failed. Mount aborted. Code: {BAN_CODE}
TECHNICAL|STOP: {BAN_CODE} ({PARAM1}, {PARAM2}, {PARAM3}, {PARAM4})
TECHNICAL|IRQL_NOT_LESS_OR_EQUAL - A driver accessed pageable memory at DISPATCH_LEVEL
TECHNICAL|PAGE_FAULT_IN_NONPAGED_AREA - Invalid memory reference at {FAULT_ADDR}
TECHNICAL|KERNEL_DATA_INPAGE_ERROR - I/O error reading kernel data. Status: {IO_STATUS}
TECHNICAL|UNEXPECTED_KERNEL_MODE_TRAP - Trap {TRAP_NUMBER} at {FAULT_ADDR}
TECHNICAL|KMODE_EXCEPTION_NOT_HANDLED - Exception {EXCEPTION_CODE} in {MODULE_NAME}
TECHNICAL|SYSTEM_THREAD_EXCEPTION_NOT_HANDLED in {MODULE_NAME}+{OFFSET}
TECHNICAL|CRITICAL_OBJECT_TERMINATION - Process {PROCESS_NAME} (PID {PID}) terminated
TECHNICAL|DRIVER_IRQL_NOT_LESS_OR_EQUAL - {DRIVER_NAME} at {FAULT_ADDR}
REASSURING|Don't worry - your files are safe. The system stopped to protect your data.
REASSURING|This is a known issue and will be fixed in the next update. Please restart.
REASSURING|The system stopped to prevent damage. Your data should be intact.
REASSURING|A temporary hardware glitch may have caused this. Try restarting.
REASSURING|This crash has been logged. The Sentinel recovery system will attempt auto-repair.
REASSURING|Recovery mode is available. Press F8 during boot to access it.
REASSURING|The BANHammer caught this error before it could cause permanent damage.
REASSURING|Automatic recovery will attempt to fix this on the next boot.
HUMOROUS|The kernel has decided to pursue other career opportunities.
HUMOROUS|Fatal error: ran out of coffee. Please refill and try again.
HUMOROUS|Segmentation fault (core dumped). The core sends its regards.
HUMOROUS|The bits are revolting! They demand better working conditions!
HUMOROUS|ERROR: Cannot divide by zero. Mathematics has not changed since last time.
HUMOROUS|The hamster powering your CPU has stopped running. Please feed it.
HUMOROUS|Achievement Unlocked: Kernel Crash (Difficulty: Easy, apparently)
HUMOROUS|To err is human. To crash spectacularly is OpenWindows.
HUMOROUS|The kernel made a boo-boo. A very serious, system-halting boo-boo.
HUMOROUS|Roses are red, violets are blue, the kernel has panicked, and so should you.
"@
Write-CFile "$Root\Artifacts\BanHammerTexts.txt" $BanHammerTexts

# Additional BanHammer config
$BanConfig = @"
# BANHammer Configuration
# Controls crash screen behavior and recovery

[display]
text_color = 0xFFFFFF
bg_color = 0x1A0A2E
accent_color = 0xFF6B6B
font_size = 16
show_technical = true
show_qr_code = false
animation_enabled = true

[recovery]
auto_restart = false
auto_restart_delay = 30
dump_memory = true
dump_path = /var/crash/
max_dumps = 10
sentinel_enabled = true

[reporting]
log_to_disk = true
log_path = /var/log/banhammer.log
max_log_size = 1048576
verbose = false

[sounds]
crash_beep = true
beep_frequency = 440
beep_duration = 500
"@
Write-CFile "$Root\Config\banhammer.conf" $BanConfig
Write-Host "  [CONF] banhammer.conf"

# ═══════════════════════════════════════════════════════════════
# REGISTRY DEFAULTS
# ═══════════════════════════════════════════════════════════════

$RegDefaults = @"
# OpenWindows KConf Registry Defaults
# Format: KEY=VALUE (flat namespace, dot-separated hierarchy)

system.kernel.version=0.1.0-alpha
system.kernel.arch=x86_64
system.kernel.build=freestanding-c99
system.kernel.debug=false
system.kernel.verbose=false
system.kernel.max_procs=1024
system.kernel.max_threads=4096
system.kernel.tick_rate=1000
system.kernel.preemptive=true

system.memory.page_size=4096
system.memory.max_physical=0xFFFFFFFF
system.memory.heap_start=0x00100000
system.memory.stack_size=65536

system.display.width=1920
system.display.height=1080
system.display.depth=32
system.display.refresh=60
system.display.vsync=true

system.console.rows=25
system.console.cols=80
system.console.color_fg=0xCCCCCC
system.console.color_bg=0x0A0A0A
system.console.cursor_blink=true

system.network.hostname=openwindows
system.network.dhcp=true
system.network.dns_primary=8.8.8.8
system.network.dns_secondary=8.8.4.4
system.network.firewall=true

system.security.require_password=true
system.security.lockout_attempts=5
system.security.session_timeout=1800
system.security.audit_enabled=false

system.gui.theme=dark
system.gui.animations=true
system.gui.font_family=default
system.gui.font_size=14
system.gui.taskbar_position=bottom
system.gui.desktop_icons=true

system.audio.master_volume=75
system.audio.mute=false
system.audio.default_device=0

system.locale.language=en
system.locale.country=US
system.locale.timezone=UTC
system.locale.charset=superunicode

system.power.suspend_timeout=600
system.power.hibernate_timeout=3600
system.power.screen_off_timeout=300
"@
Write-CFile "$Root\Config\registry_defaults.kconf" $RegDefaults
Write-Host "  [CONF] registry_defaults.kconf"

# ═══════════════════════════════════════════════════════════════
# SPECIFICATION DOCS
# ═══════════════════════════════════════════════════════════════

$Specs = @(
    @{ Name="OWX_SPECIFICATION.md"; Content="# OWX Native Executable Format Specification`n`n## Overview`nThe `.owx` format is the native executable binary format for OpenWindows.`n`n## Header Layout`n| Offset | Size | Field |`n|--------|------|-------|`n| 0x00 | 4 | Magic (0x4F575831) |`n| 0x04 | 4 | Version |`n| 0x08 | 4 | Flags |`n| 0x0C | 4 | Entry Point |`n| 0x10 | 4 | Section Count |`n| 0x14 | 4 | Import Count |`n| 0x18 | 4 | Export Count |`n| 0x1C | 8 | Image Size |`n| 0x24 | 4 | Stack Size |`n| 0x28 | 4 | Heap Size |`n| 0x2C | 4 | Checksum |`n| 0x30 | 4 | Subsystem |`n| 0x34 | 204 | Reserved |`n`nTotal header: 256 bytes`n" }
    @{ Name="DRIVER_ARCHITECTURE.md"; Content="# OpenWindows Driver Architecture`n`n## Driver Types`n- **Bus Drivers** (pcibridge, i2c, spi)`n- **Storage Drivers** (ahci, nvmedrv, atapio, floppy, cdrom)`n- **Network Drivers** (e1000, rtl8139, ipstack, tcpcore, udpcore)`n- **Input Drivers** (ps2hid, mousedrv, kbdlayout, i8042)`n- **Display Drivers** (vgapci, vbedrv, fbdev, virtio_gpu)`n- **Audio Drivers** (ac97, hdaudio)`n- **System Drivers** (acpitbl, ioapic, lapic, hpet, smbios, cmos, rtc)`n- **Virtual Drivers** (virtio_blk, virtio_net, ramdisk, loopback, devnull)`n- **Security Drivers** (pfwall, entropy, watchdog)`n`n## Driver Lifecycle`n1. owc_entry_init() - Initialize driver`n2. owc_entry_start() - Begin operation`n3. owc_entry_stop() - Cease operation`n4. owc_entry_cleanup() - Free resources`n" }
    @{ Name="NETWORKING_STACK.md"; Content="# OpenWindows Networking Stack`n`n## Layer Architecture`n````nApplication Layer  : owsock64, owtls64`nTransport Layer    : tcpcore, udpcore`nNetwork Layer      : ipstack, pfwall`nLink Layer         : e1000, rtl8139, virtio_net`n````n`n## Services`n- DHCP Client (dhcpcli)`n- DNS Resolver (dnscli)`n- Packet Filter Firewall (pfwall)`n" }
    @{ Name="GUI_ARCHITECTURE.md"; Content="# OpenWindows GUI Architecture`n`n## Component Stack`n````nApplications   : owterm, owfm, ownotepad, owpaint, owcalcgui`nWidgets        : owmenu64, owdlg64, owicon64, owdnd64, ownotif64`nWindow Manager : owwm64 + owwm`nRendering      : owgfx64, owfont64, sufrender`nDisplay        : fbdev, vbedrv, bootvid`n````n`n## Theme System`n- Managed by owtheme64`n- Default theme: dark mode`n- Theme files stored in /etc/themes/`n" }
    @{ Name="SECURITY_MODEL.md"; Content="# OpenWindows Security Model`n`n## Components`n- **owacl64** - Access Control Lists`n- **owcrypt** - Cryptographic primitives`n- **entropy** - Hardware random number generation`n- **kext_crypto** - Kernel crypto extension`n- **kext_sandbox** - Process sandboxing`n- **kext_audit** - Security audit logging`n- **pfwall** - Network packet filtering`n`n## User Authentication`n- Managed by owlogin / owpasswd`n- Sessions tracked by kernel`n- Privilege elevation via owsu`n" }
)

foreach ($spec in $Specs) {
    Write-CFile "$Root\Specs\$($spec.Name)" $spec.Content
    Write-Host "  [SPEC] $($spec.Name)"
}

# ═══════════════════════════════════════════════════════════════
# SUMMARY
# ═══════════════════════════════════════════════════════════════

$totalDrivers = $NewDrivers.Count
$totalDLLs = $NewDLLs.Count
$totalApps = $NewApps.Count
$totalScripts = $Scripts.Count
$totalCaches = $CacheHeaders.Count
$totalFormats = $BinaryFormats.Count
$totalKTexts = $KTexts.Count
$totalKExts = $KExts.Count

$totalNew = ($totalDrivers * 2) + ($totalDLLs * 2) + $totalApps + $totalScripts + $totalCaches + $totalFormats + $totalKTexts + ($totalKExts * 2) + $Specs.Count + 3  # +3 for BanHammerTexts, banhammer.conf, registry_defaults

Write-Host ""
Write-Host "═══════════════════════════════════════════"
Write-Host "  BATCH GENERATION COMPLETE"
Write-Host "═══════════════════════════════════════════"
Write-Host "  New Drivers:       $totalDrivers ($($totalDrivers * 2) files)"
Write-Host "  New DLLs:          $totalDLLs ($($totalDLLs * 2) files)"
Write-Host "  New Applications:  $totalApps ($totalApps files)"
Write-Host "  Kernel Scripts:    $totalScripts files"
Write-Host "  Cache Headers:     $totalCaches files"
Write-Host "  Binary Formats:    $totalFormats files"
Write-Host "  Kernel Texts:      $totalKTexts files"
Write-Host "  Kernel Extensions: $totalKExts ($($totalKExts * 2) files)"
Write-Host "  Specs/Docs:        $($Specs.Count) files"
Write-Host "  Config Files:      3 files"
Write-Host "  ─────────────────────────────────────────"
Write-Host "  TOTAL NEW FILES:   $totalNew"
Write-Host "═══════════════════════════════════════════"

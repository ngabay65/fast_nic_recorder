1. Overview
2. Machine requirement
3. OS Linux requirement
4. Howto capture Ethernet packets to memory
5. Howto capture Ethernet packets to memory and record to hard disks
6. Description of user space applications.
7. Frequent questions and answers


1. Overview
   This software package provides 100% of source code, to capture 10 gigabits 
   Ethernet traffic to memory and record captured data to disks.

   This software system reserves private memory not visible to Linux OS.
   Reserved memory is partitioned into multiple memory packets of one gigabytes
   each.

   Custom NIC driver will fill captured Ethernet traffic into memory buckets.

   At the same time, hard disk writers then write multiple memory buckets into 
   multiple hard drives.
   
2. Machine requirement
   Machine must be capable of running 64-bit Linux and has sufficient amount
   of memory. Memory for Linux OS is usually between 4 to 8 gigabytes.

   Aside from memory for Linux OS, private memory for capture data is also 
   needed.

   Private memory is divided to 1-gigabyte buckets. The number of memory buckets
   has to be slightly bigger than the number of "data writers".

   Each data writer writes one private memory bucket at a time to a specific
   hard drive designated for that writer.

   All data writers running in parallel have to be fast enough to store roughly
   one gigabyte of data in the worst case.

   ** Example of howto reserved private memory.
   Assume we have a 32 gigabytes memory system, and reserve 10 gigabytes for
   Linux OS.

   Goto /boot directory.
   Depend on whether you have "grub" or "grub2" boot loader, the directories
   can be

/boot/grub/grub.cfg
or
/boot/grub2/grub.cfg
or
/boot/efi/EFI/redhat/grub.conf

   Locate the kernel we are using. In our case, it would be
	3.3.4-5.no_soft_irq.fc17.x86_64

   This kernel will be installed in the next section. So hold off doing this 
   until we install our custom Linux kernel.

   Modify the "grub" config text file.
   Search for the entry:

	kernel /vmlinuz-3.3.4-5.no_soft_irq.fc17.x86_64

   Go to this end of this line and append ==>

	mem=10G

   After reboot, we will see Linux OS only uses 10 gigabytes, despite we have
   32 gigabytes of memory. You can check for memory Linux uses:

	cat /proc/meminfo | more

	or

	cat /proc/iomem

   ** We need one pci-e x8 Gen 2 or Geni 3 slot, to use for the ixgbe adapter.
      We use the Silicom dual ports Gen 2 adapters.

   ** If you need more than a couple hard drives, then we need another pci-e
      slot, to use the SAS/SATA controller.

3. OS Linux requirement
   A custom built Linux OS is needed, so that custom kernel threads can be run
   forever without giving up CPU cores. (and roughly no one will interrupt
   these CPU cores as well).

   With a standard Linux version, after so many seconds, soft IRQ lockup
   warning will be printed.

   To avoid this warning, custom Linux kernel has to be built with the following
   configuration turned off:

# CONFIG_LOCKUP_DETECTOR is not set
# CONFIG_HARDLOCKUP_DETECTOR is not set

   If you use your own OS, you need to built a custom kernel described above.

   This package elects to use Fedora core 17. And we have custom kernel images
   ready.

	kernel-3.3.4-5.no_soft_irq.fc17.x86_64.rpm
	kernel-devel-3.3.4-5.no_soft_irq.fc17.x86_64.rpm
	kernel-headers-3.3.4-5.no_soft_irq.fc17.x86_64.rpm

   To use this pre-built Linux images, install the standard Fedora 17 from DVD.
   Download the above three RPM packages from our web site.
   Unpack them:

	rpm -ivh kernel-3.3.4-5.no_soft_irq.fc17.x86_64.rpm
	rpm -ivh kernel-devel-3.3.4-5.no_soft_irq.fc17.x86_64.rpm
	rpm -ivh kernel-headers-3.3.4-5.no_soft_irq.fc17.x86_64.rpm

   Reboot the PC, and boot this kernel.

   Reserve private memory as described in section [2].
 
4. Howto capture Ethernet packets to memory
   It makes sense to capture into memory first, without writing to hard drives.
   This step is critical, to make sure we can capture at line rate, with the
   smallest possible packet size. The smallest packet size is 64 bytes.
   (60 bytes data + 4 bytes checksum). This will result to 14.88 million
   packets per second.

   You need a traffic generator to do this. For hardware, there are products
   like Smartbits or Ixia. For software, see that you can get a package from
   www.ntop.org.

   ** configure private memory for our system.
      We have successfully reserved private memory in step [3] or [2].
      In the example, we reserve 10G for Linux. Therefore, we have 22 gigabytes
      for private memory, or 22 buckets of one gigabytes each.
      Bucket will be numbered from 10 to 31.

      Go to the header file,

	./nlsim_ixgbe-1.00/ixgbe-3.18.7/src/nlsim_ixgbe.h

      Even though this header file is in ixgbe driver directory, it is actually
      shared by all drivers and user space applications.

      Search and modify the following

#define START_1G        10
#define END_1G          31

   ** build everything

      ./nlsim_ixgbe-1.00>./build_all

   ** capture to memory procedure

      window#1 nlsim_ixgbe-1.00>./init_capture
repeat_again:
      window#1 nlsim_ixgbe-1.00>./driver_ixgbe

      window#2 nlsim_ixgbe-1.00/application_ixgbe>./count_then_discard


      window#1 nlsim_ixgbe-1.00>./start

      Start traffic generator, generate a couple hundred million packets.
      Stop traffic generator.

      window#1 nlsim_ixgbe-1.00>./stop
         (note: when typing "./stop", do not hit tab key, as sometime it will
                make the PC hangs.)

      window#1 nlsim_ixgbe-1.00>ifconfig nam0
         Too see if there is any loss packets.

      window#1 nlsim_ixgbe-1.00>rmmod ixgbe
         Have to remove the driver, before capturing again.
         To capture again, starting from "repeat_again:".

   ** discussion
      Not all PC can be captured at line rate.
      I have two PCs.

      The Dell XPS 8500 does line rate. But it only has one pci express slot 
      x16 and is being used for graphics card. I have to replace this x16 
      graphics card with the x1 graphics card. The available x16 slot is then 
      used for the Silicom 10G adapter.
      This system does not have enough hard drive connectors or pci express
      slot to insert another hard drive controller. Not fast enough to store
      one gigabytes of data per second to disk.
      We also replace the stock hard drive with a Samsung EVO 240G. It can
      sustain a 3-gigabit data flow.

      The second system is an Intel i7 with six cores, with some of the most 
      expensive parts. But it can only capture without loss at 12 million 
      packets per second. Still we can generate 10G line rate traffic with 
      84-byte packets, in order to test "parallel data writers" system. 
      We have four Samsung pro 840. Two connect to 6G sata. The other two 
      connect to 3G sata.
      The parallel data writer system is fast enough to write 10G data traffic.
   
5. Howto capture Ethernet packets to memory and record to hard disks
   *** configure "parallel data writer" system
       The system we test have four SSDs.
           Two of them
		/mnt/sda
		/home/phongle/sdb
	   have the transfer rate of roughly 500MB per second.
           Two of them
		/mnt/sdc
		/mnt/sdd
	    have the transfer rate of roughly 250MB per second.

       The system have 22 buckets which can allow upto 20 writers.

       We use 12 writers. 
	    Four writers share /mnt/sda.
	    Four writers share /home/phongle/sdb.
	    Two writers share /mnt/sdc.
	    Two writers share /mnt/sdd.

       Data writers and all other user space applications learn about this
       configuration via ./nlsim_ixgbe-1.00/application_ixgbe/writer_info.cfg

       The content is:
		12 50
		0 /mnt/sda
		1 /home/phongle/sdb
		2 /mnt/sdc
		3 /mnt/sdd
		4 /mnt/sda
		5 /home/phongle/sdb
		6 /mnt/sda
		7 /home/phongle/sdb
		8 /mnt/sdc
		9 /mnt/sdd
		10 /mnt/sda
		11 /home/phongle/sdb

       "12 50" means there are 12 data writers. Each writer writes a maximum
       of 50 buckets (50 Gbytes).

       12 writers is indexed from writer#0 to writer#11.
       Each writer can only write to one specific hard drive.

       So sda and sdb should have room for 200 Gbytes.
       sdc and sdd should have room for 100 Gytes.

       This arrangement will store 600 buckets (600 Gbytes).

       12 writers will be launched by the following script:
       Script name = ./nlsim_ixgbe-1.00/application_ixgbe/write2disk_debug
		./todisk 0 debug &
		./todisk 1 debug &
		./todisk 2 debug &
		./todisk 3 debug &
		./todisk 4 debug &
		./todisk 5 debug &
		./todisk 6 debug &
		./todisk 7 debug &
		./todisk 8 debug &
		./todisk 9 debug &
		./todisk 10 debug &
		./todisk 11 debug &
	Without "debug" argument, writers will go silent and not display
        disk throughput.

   *** discussion on how big other commercial system stores data
       Some SAS/SATA controllers have 6 ports. Each port can hook up 16 disks.
       If they use 3-terabyte disk, the system is 6 x 16 x 3 = 288 Tbytes.
       
       Can we do the same thing? Yes.
       (way 1) without changing any source code, select a PC with 128 Gbytes.
       Reserve 20G for Linux OS. We will have 108 private memory buckets.
       Configure our "parallel data writer" system with 6 x 16 = 96 writers.

       (way 2) assume we have only 32G, 10G for Linux, 22 buckets.
       Our kernel driver stays the same, e.g. write to 22 buckets one-by-one
       Modify "writer_info.cfg" to have 6 tables, each table manage 16 disks.
       Modify "todisk" to handle table#1. When disks in table#1 is full,
       switch to table#2, then 3,4,5,6.

   *** discussion on how to store data forever
       Most SAS/SATA controllers have "hot plug" capability. When a disk is
       removed, and another disk is inserted, the hot plug driver assigns the
       new disk another name.

       It is possible to modify to controller source code, to reuse old disk
       names. If we do so. And assume that we have two JBODs (Just a Bunch Of
       Disks), each JBOD has 48 disks, our system can use one JBOD, while the
       other JBOD can be swapped with 48 new disks.

   *** running "parallel data writer" system

      window#1 nlsim_ixgbe-1.00>./init_capture

      window#1 nlsim_ixgbe-1.00>./driver_ixgbe

      window#2 nlsim_ixgbe-1.00/application_ixgbe>./write2disk_debug

      window#1 nlsim_ixgbe-1.00>./start

      Start traffic generator.
      Stop traffic generator.

      window#1 nlsim_ixgbe-1.00>./stop
         (note: when typing "./stop", do not hit tab key, as sometime it will
                make the PC hangs.)

      window#1 nlsim_ixgbe-1.00>ifconfig nam0
         Too see if there is any loss packets.

      window#1 nlsim_ixgbe-1.00>rmmod ixgbe
         Have to remove the driver, before capturing again.
         To capture again, starting from "repeat_again:".

   *** note: "parallel writer system" can detect "stalling". It happens when
       writer system can not keep up with capture data rate. When this case
       happens, all writers will stop.

6. Description of user space applications.
   Applications we use so far include "start", "stop", "count_then_discard",
   "todisk". The rest are:

   "status"		Display current values of start, stop.
   "dump_packet <start> <count>"        
			Display hex dump of packets given (1) starting packet
 			number (2) number of packets to display.
   "count_recorded"	Starting from first recorded bucket to last recorded
			bucket, display bucket information: (1) number of
                        packets in that bucket (2) accumulate number of packets
			since first bucket (3) Date and time this bucket is
                        written to disk.
   "to_libpcap <start_packet> <packet_count> <pcap_file_name>"
			Extract a number of packets and write to a libpcap file
                        so that it can be view from wireshark. Parameters (1)
                        starting packet number (2) number of packets to extract.
   "block_info <bucket number>"
			Same as count_recorded, but only for one bucket, not
                        all.
   "block2libpcap <start_bucket#> <packet_count> <pcap_file_name>"
			Same as to_libpcap, but using starting bucket number,
                        instead of starting packet number.
	
7. Frequent questions and answers
   Some of these can be "google". I did the same thing.

  (1) Question: When loading nlsim driver, the Linux OS complains that a driver
      already loaded! What should I do?

      Answer: When any Linux OS gets installed on a computer, all related I/O 
      drivers are stored in special directories starting with:

	/lib/modules/3.3.4-5.no_soft_irq.fc17.x86_64/..

      The standard ixgbe driver is at:
	/lib/modules/3.3.4-5.no_soft_irq.fc17.x86_64/kernel/drivers/net/
	ethernet/intel/ixgbe/ixgbe.ko

      This driver is loaded when during the boot process.
      Even if you remove it, it will be loaded again very soon.

      There is some nice way under Linux to prevent this from happening.

      But my way is:
        (a) save ixgbe.ko in some place, in case we need to use it.
        (b) in directory /lib/modules/...., rename the file to ixgbe.kk

      This should work.

  (2) Question: When loading nlsim driver, how do I know which physical Ethernet
      port nlsim is using if I have more than one ixgbe ports?

      Answer: nlsim only use one port, "nam0". If you have more than one port,
      "nam1", and so on can be used as normal Linux interfaces.

      After you issue two commands: "./init_capture" and "./driver_ixgbe", do:
	>dmesg | grep nam0

      If you see
] ixgbe 0000:01:00.0: nam0: [nlsim]NIC Link is Up 10 Gbps, Flow Control: RX

      then you have plugged cable into the correct port.

      If you don't see "nam0: [nlsim]", remove the cable, and plug into other
      ixgbe ports, until you see the above message.

      In our system, we also have two ixgbe ports. "nam1" can be used as 
      standard Linux interface, and dmesg show the following:

] ixgbe 0000:01:00.1: nam1: [xxxxx]NIC Link is Up 10 Gbps, Flow Control: RX/TX

      If you see "nam?: [xxxxx]", then that "nam?" is a standard Linux
      interface.

  (3) Question: Why don't nlsim driver name interface as "eth" instead of "nam"

      Answer: Some Linux OS including Fedora renames "eth" to be like "p4p1" or
      "p3p1". And it is inconsistent and different from computer to computer.
      
      Changing it to "nam" which is different from "eth" prevents Linux to
      rename ixgbe interface. Thus the same scripts that we write will work on
      any installation.

  (3) Question: I have a number of hard drives connected to a number of SAS/SATA
      ports. Some ports are 3Gbps some are 6Gbps. And Linux only displays
      hard drive names as /dev/sda, /dev/sdb, and so on.
      How do I relate, for example, /dev/sdb, to which physical hard drive?
      In turn, I can relate /dev/sdb to which 3G or 6G SAS/SATA port?

      Answer: install "hdparm" utility.
      Type this command:
      >hdparm -i /dev/sdb

      It will display SerialNo, and this number also gets printed on your
      hard drive.

      Once you locate the hard drive, thru the cable connected, you will know
      which port you plug this hard drive to.
  
      Refer to the motherboard manual, you will know the SATA port speed.

  (4) Question: How do I get an idea on how fast a specific hard drive?

      Answer: install "hdparm" utility.
      Type this command:
      >hdparm -tT /dev/sdb
 
      Total Mbytes per second of all hard drives, should be larger than one
      gigabytes per second.

  (5) Question: How do I know an adapter is ixgbe/82599?

      Answer: Plug the adapter into a Linux PC, power on. Then:
      >lspci -vvv | grep 82599

  (6) Question: To reserve Linux memory and private memory, how do I know
      memory size, before and after reservation?

      Answer: Your absolute total physical memory can be found:
      (a) during power up, BIOS will "usually" display memory detected.
      (b) enter BIOS, to see it.
      (c) under Linux, see it using (I) cat /proc/meminfo (II) /proc/iomem
          before reservation, it equals to amount reported by BIOS.
          after reservation, it equals to amount you limit. (mem=??G).


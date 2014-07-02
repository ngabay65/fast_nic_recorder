/* ***************************************************************
 *
 * (C) 2014 - nlsim.com
 *
 * Phong Le <phongle@nlsim.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#define NLSIM_KERNEL    1

#ifdef NLSIM_KERNEL
#include <linux/kthread.h>
#include "nlsim_ixgbe.h"

static struct ixgbe_adapter *pbm_adapter = NULL;
static struct pci_dev *pbm_pdev = NULL;
struct net_device *pbm_netdev = NULL;



static struct ixgbe_ring *pbm_rx_ring = NULL;

int nlsim_alloc_thread (void *data);
int nlsim_process_thread (void *data);

static volatile u16 next_to_clean = 0;
static volatile unsigned int max_row = 0;
static volatile unsigned char process_stop = 0, process_start = 0;

int nlsim_alloc_thread (void *data)
{
  static unsigned int glength [NUM_RX_SLOTS];
  struct ixgbe_ring *local_pbm_rx_ring = NULL;
  union ixgbe_adv_rx_desc *rx_desc;
  volatile unsigned char *fast_start, *fast_stop;
  struct bucket_info_struct *bucket_info = NULL;
  unsigned short *u16_ptr;
  unsigned long cur30;
  u16 sup_row = 0;
  register u16 sup_partial = 0;
  short count, i;
  short cur = START_1G;

  set_current_state (TASK_UNINTERRUPTIBLE);
  while (pbm_rx_ring == NULL)
    msleep (1);        

  local_pbm_rx_ring = pbm_rx_ring;
  fast_stop = &gshare->data [USER_ISSUE_STOP];
  fast_start = &gshare->data [USER_ISSUE_START];

  while (*fast_start == 0) {
    if (*fast_stop == 1) {
      process_stop = 1;
      do_exit (0);
      return 0;
    }
    msleep (1);
  }
  process_start = 1;

  cur30 = (unsigned long) cur << 30;
  sup_row = sup_partial = 0;
  bucket_info = len_array_kvirt [cur];
  u16_ptr = &bucket_info->len [0][0] - NUM_RX_SLOTS;
  rx_desc = IXGBE_RX_DESC(local_pbm_rx_ring, sup_partial);

  while (1) {
    if (unlikely(*fast_stop)) {
      process_stop = 1;
      printk ("[NLSIM] alloc_thread: ends sup_row=%d  sup_partial=%d\n", sup_row, sup_partial);
      do_exit (0);
      return 0;
    }

    // ********************
    // refill empty buffers
    // ********************
    count = (NUM_RX_SLOTS + next_to_clean - sup_partial - 1) & RX_SLOT_MASK;
    if (unlikely(!count))
      continue;

    if (count > NUM_RX_SLOTS - sup_partial)
      count = NUM_RX_SLOTS - sup_partial;
 
    if (likely(sup_row)) {
      for (i = 0; i < count; i++) {
        glength [sup_partial] += u16_ptr [sup_partial];
        if (unlikely(glength [sup_partial] > BYTE_LIMIT)) {
          max_row = bucket_info->u.real.num_captured = (sup_row + 1) << RX_SLOT_BITS;
        } 
        rx_desc->read.hdr_addr = 0;
        rx_desc->read.pkt_addr = cur30 |
 	    (BYTES_PER_SLOT * sup_partial + glength [sup_partial]); 
        rx_desc++;
        sup_partial++;
      }
    }
    else {
      for (i = 0; i < count; i++) {
        glength [sup_partial] = 0;
        rx_desc->read.hdr_addr = 0;
        rx_desc->read.pkt_addr = cur30 | BYTES_PER_SLOT * sup_partial; 
        rx_desc++;
        sup_partial++;
      }
    }

    if (sup_partial == NUM_RX_SLOTS) {
      sup_partial = 0;
      ++sup_row;
      rx_desc -= NUM_RX_SLOTS;
      u16_ptr += NUM_RX_SLOTS;
      if (max_row) {
        sup_row = 0;
        cur = (cur == END_1G) ? START_1G : cur+1;
        cur30 = (unsigned long) cur << 30;
        bucket_info = len_array_kvirt [cur];
        u16_ptr = &bucket_info->len [0][0] - NUM_RX_SLOTS;
      }
    }
    wmb();
    writel(sup_partial, local_pbm_rx_ring->tail);
  }
  do_exit (0);
  return 0;
}

int nlsim_process_thread (void *data)
{
  union ixgbe_adv_rx_desc *rx_desc;
  struct bucket_info_struct *store_bucket_info = NULL;
  unsigned short *u16_ptr;
  struct ixgbe_ring *local_pbm_rx_ring = NULL;
#ifdef IFCONFIG_BYTE_COUNT
  unsigned int total_bytes = 0;
#endif
  register u32 cur_partial = 0;
  short cur = START_1G;

  set_current_state (TASK_UNINTERRUPTIBLE);
  while (pbm_rx_ring == NULL)
    msleep (1);        
  local_pbm_rx_ring = pbm_rx_ring;

  while (process_start == 0) {
    if (process_stop == 1) {
      do_exit (0);
      return 0;
    }
    msleep (1);
  }

  msleep (10);
  store_bucket_info = len_array_kvirt [cur];
  u16_ptr = &store_bucket_info->len [0][0];
  rx_desc = IXGBE_RX_DESC(local_pbm_rx_ring, 0);

  while (likely(!process_stop)) {
    // ************************
    // process received packets
    // ************************
    prefetch(rx_desc);

    while (1) {
      if (unlikely(!ixgbe_test_staterr(rx_desc, IXGBE_RXD_STAT_DD)))
        break;

      rmb();

      u16_ptr [cur_partial] = rx_desc->wb.upper.length;
#ifdef IFCONFIG_BYTE_COUNT
      total_bytes += u16_ptr [cur_partial];
#endif

      rx_desc++;
      prefetch(rx_desc);

      if ((++cur_partial & 0xff) == 0) {
        next_to_clean = cur_partial;
        if ((cur_partial & RX_SLOT_MASK) == 0) {
          rx_desc -= NUM_RX_SLOTS;
          prefetch (rx_desc);
          if (max_row == cur_partial) {
#ifdef IFCONFIG_PACKET_COUNT
            local_pbm_rx_ring->stats.packets += cur_partial;
#endif
#ifdef IFCONFIG_BYTE_COUNT
            local_pbm_rx_ring->stats.bytes += total_bytes;
            total_bytes = 
#endif
            max_row = cur_partial = 0;
            cur = (cur == END_1G) ? START_1G : cur+1;
            store_bucket_info = len_array_kvirt [cur];
  	    u16_ptr = &store_bucket_info->len [0][0];
          }
        }
      }
    }
    // next_to_clean = cur_partial;
  }
  printk ("[NLSIM2] clean_thread: stops num_captured=%d\n", cur_partial);
  store_bucket_info->u.real.num_captured = cur_partial;
  store_bucket_info->u.real.last = 1;
#ifdef IFCONFIG_PACKET_COUNT
  local_pbm_rx_ring->stats.packets += cur_partial;
#endif
#ifdef IFCONFIG_BYTE_COUNT
  local_pbm_rx_ring->stats.bytes += total_bytes;
#endif
  do_exit (0);
  return 0;
}
#endif

#ifdef NLSIM_KERNEL
struct task_struct *task_alloc, *task_process;

char name [128][32] =
{
"buc000", "buc001", "buc002", "buc003", "buc004", "buc005", "buc006", "buc007",
"buc008", "buc009", "buc010", "buc011", "buc012", "buc013", "buc014", "buc015",
"buc016", "buc017", "buc018", "buc019", "buc020", "buc021", "buc022", "buc023",
"buc024", "buc025", "buc026", "buc027", "buc028", "buc029", "buc030", "buc031",
"buc032", "buc033", "buc034", "buc035", "buc036", "buc037", "buc038", "buc039",
"buc040", "buc041", "buc042", "buc043", "buc044", "buc045", "buc046", "buc047",
"buc048", "buc049", "buc050", "buc051", "buc052", "buc053", "buc054", "buc055",
"buc056", "buc057", "buc058", "buc059", "buc060", "buc061", "buc062", "buc063",
"buc064", "buc065", "buc066", "buc067", "buc068", "buc069", "buc070", "buc071",
"buc072", "buc073", "buc074", "buc075", "buc076", "buc077", "buc078", "buc079",
"buc080", "buc081", "buc082", "buc083", "buc084", "buc085", "buc086", "buc087",
"buc088", "buc089", "buc090", "buc091", "buc092", "buc093", "buc094", "buc095",
"buc096", "buc097", "buc098", "buc099", "buc100", "buc101", "buc102", "buc103",
"buc104", "buc105", "buc106", "buc107", "buc108", "buc109", "buc110", "buc111",
"buc112", "buc113", "buc114", "buc115", "buc116", "buc117", "buc118", "buc119",
"buc120", "buc121", "buc122", "buc123", "buc124", "buc125", "buc126", "buc127"
};

int pbm_init_all (void)
{
  struct bucket_info_struct *bucket_info;
  int i;

  for (i = 0; i < 128; i++) {
    len_array_phys [i] = 0;
    base_ptr [i] = ((unsigned long) i << 30);
  }

#if 0
  if (END_1G - START_1G + 1 < 4) {
    printk ("RAM is too small\n");
    return -1; 
  }
#endif

#if 1
  for (i = START_1G; i <= END_1G; i++) {
    len_array_phys [i] = base_ptr [i] + 992 * CONST_1M;
    if (request_mem_region ((unsigned long) base_ptr [i], CONST_1G, name [i]) == NULL) {
      printk ("request_memory_region[%d] start=%lx size=%x failed\n", i, (unsigned long) base_ptr [i], CONST_1G);
    }
    len_array_kvirt [i] = ioremap ((resource_size_t) len_array_phys [i], (unsigned long) 32 * CONST_1M);

    if (len_array_kvirt [i] == NULL) {
      printk ("len_array_kvirt [%d] == NULL\n", i);
      return -1;
    }
    else {
      printk ("len_array_kvirt [%d]=%lx  len_array_phys [%d]=%lx\n", i, (unsigned long) len_array_kvirt [i], i, (unsigned long) len_array_phys [i]);
      bucket_info = (struct bucket_info_struct *) len_array_kvirt [i];
      memset (&bucket_info->u.real, 0, sizeof (bucket_info->u.real));
      bucket_info->u.real.num_rx_slots = NUM_RX_SLOTS;
    }
  }
#endif

  global_phys = GLOBAL_ADDR;
  global_kvirt = len_array_kvirt [START_1G] + 32 * CONST_1M - 4096;
  printk ("global_kvirt=%lx  global_phys=%lx phys2=%lx\n", (unsigned long) global_kvirt, (unsigned long) global_phys, (unsigned long) len_array_phys [START_1G] + 32 * CONST_1M - 4096);

  gshare = (struct global_share_struct *) global_kvirt;

  gshare->data [USER_ISSUE_START] = 0;
  gshare->data [USER_ISSUE_STOP] = 0;

  gshare->magic1 = MAGIC1;
  gshare->magic2 = MAGIC2;

  task_alloc = kthread_create (nlsim_alloc_thread, NULL, "nlsim_alloc");
  if (IS_ERR(task_alloc)) {
    printk ("error: run nlsim_alloc_thread\n");
    return PTR_ERR(task_alloc);
  }
  kthread_bind (task_alloc, 5);
  wake_up_process (task_alloc);

  task_process = kthread_create (nlsim_process_thread, NULL, "nlsim_process");
  if (IS_ERR(task_process)) {
    printk ("error: run nlsim_process_thread\n");
    return PTR_ERR(task_process);
  }
  kthread_bind (task_process, 4);
  wake_up_process (task_process);

  printk ("pbm_init_all() done\n");
  return 0;
}
#endif

#ifdef NLSIM_KERNEL
int pbm_destroy_all (void)
{
  int i;
  gshare->magic1 = ~MAGIC1;
  gshare->magic2 = ~MAGIC2;

  for (i = START_1G; i <= END_1G; i++) {
    iounmap ((void *) len_array_kvirt [i]);
    release_mem_region (base_ptr [i], CONST_1G);
  }

  return 0;
}
#endif


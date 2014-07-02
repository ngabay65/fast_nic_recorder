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

#ifndef NLSIM_IXGBE_HEADER
#define NLSIM_IXGBE_HEADER

/* ******** configurable constant ******** */
#define NLSIM_PORT_USE	(0)
#define NLSIM_MAJOR	200

/* valid values = 12,13,14,15 */
#define RX_SLOT_BITS	12

// #define IFCONFIG_PACKET_COUNT
// #define IFCONFIG_BYTE_COUNT

// dell xps 8500 memory map
// #define START_1G	8
// #define END_1G		15

// digital storm memory map
#define START_1G	10
#define END_1G		24

#define GLOBAL_ADDR     ((((unsigned long) START_1G + 1) << 30) - 4096)
#define LEN_BLOCK_SIZE	(32 * 1024 * 1024 - 4096)

#define MAX_WRITERS	(1024)

/* *************************************** */



#define NUM_RX_SLOTS	(1 << RX_SLOT_BITS)

#ifdef NLSIM_KERNEL
#include "ixgbe.h"
#if (IXGBE_DEFAULT_RXD!=NUM_RX_SLOTS)
#error "IXGBE_DEFAULT_RXD (in ixgbe.h) != NUM_RX_SLOTS (in nlsim_ixgbe.h)"
#error "for NUM_RX_SLOTS==(1 << RX_SLOT_BITS), please adjust RX_SLOT_BITS"
#endif
#endif

#define RX_SLOT_MASK	(NUM_RX_SLOTS-1)

#if (RX_SLOT_BITS==12)
#define BYTES_PER_SLOT	(240 * 1024)
#endif

#if (RX_SLOT_BITS==13)
#define BYTES_PER_SLOT	(120 * 1024)
#endif

#if (RX_SLOT_BITS==14)
#define BYTES_PER_SLOT	(60 * 1024)
#endif

#if (RX_SLOT_BITS==15)
#define BYTES_PER_SLOT	(30 * 1024)
#endif

// #define BYTES_PER_SLOT		BYTES_PER_SLOT_4K
#define RX_BUF_SIZE		1514
#define BYTE_LIMIT		(BYTES_PER_SLOT - 2 * RX_BUF_SIZE)

#define CONST_1K	(1024)
#define CONST_1M	(1024 * 1024)
#define CONST_1G	(1024 * 1024 * 1024)

/*
 *   128 max. gigabytes
 *   16M packets per gig
 *
 *
 *per Gig
 *
 *(1) 32M
 *    (a) bucket_struct     unsigned short [4096] == 8K bytes
 *    (b) 32M - unsigned short [4096].
 *        == [4095][4096]
 *(2) 992M
*/

/* unsigned short (*len_array) [128][4096][4096]; */
// 32M x 16
// 16M x 32 
// 8M x 64
// 4M x 128
#ifdef NLSIM_KERNEL
dma_addr_t len_array_phys [128];
#else
unsigned long len_array_phys [128];
#endif
void *len_array_kvirt [128];

// #define INVALID_ROW	(1024 * 1024)

struct bucket_info_struct {
  union {
    unsigned short pad1 [4096];
    struct real_struct {
      unsigned char user_stamp [64];
      unsigned int num_captured;	// num_full_row X 4096 + num_partial
      unsigned int last;
      unsigned int stalled;
      unsigned int num_rx_slots;
    } real;
  } u;
  unsigned short len [(4096 * 4096) >> RX_SLOT_BITS][NUM_RX_SLOTS];
};
 
#ifdef NLSIM_KERNEL
dma_addr_t global_phys;
#else
unsigned long global_phys;
#endif
unsigned short *global_kvirt;

#ifdef NLSIM_KERNEL
dma_addr_t base_ptr [128]; 
#else
unsigned long base_ptr [128]; 
#endif

#define MAGIC1	0xDEADBABE
#define MAGIC2	0xFF00AABB
/*
global struct
driver init this struct
*/
#define USER_ISSUE_START	0
#define USER_ISSUE_STOP		1
struct global_share_struct {
  unsigned int magic1;
  unsigned int magic2;
  unsigned char data [16];
} *gshare;

#endif

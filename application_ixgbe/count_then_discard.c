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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h> 

#include "../ixgbe-3.18.7/src/nlsim_ixgbe.h"

#define printk		printf

struct bucket_info_struct *bucket_info = NULL;

int is_magic_ok (struct global_share_struct *gshare)
{
  if (gshare->magic1 == MAGIC1 && gshare->magic2 == MAGIC2)
    return 1;
  return 0;
}

int read_user_stop (struct global_share_struct *gshare)
{
    int temp;

    temp = gshare->data [USER_ISSUE_STOP];
// printf ("xxxxxxxxxxxxxx temp = %d\n", temp);
    return temp;
}

void sleep_100ms (void)
{
  struct timespec tim, delay;

  delay.tv_sec = 0;
  delay.tv_nsec = 100000000;

  nanosleep (&delay, &tim);
}

int main (int argc, char *argv[])
{
    int fd;
    int cur, end_cnt;
    unsigned long total_packets = 0L, packets = 0L;
    int delay_stop = 0;
    int driver_stop = 0;

    if (argc != 1) {
      printf ("count_then_discard\n");
      return 0;
    }

    fd = open ("/dev/nlsim", O_RDWR);
    if (fd == -1) {
        perror ("open");
        return 1;
    }

    gshare = (struct global_share_struct *) mmap ((void *) ((unsigned long) GLOBAL_ADDR), 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (gshare == MAP_FAILED) {
        perror ("mmap");
        return 1; 
    }

    // do_capture_start ();
    // wait for magic
    while (is_magic_ok (gshare) == 0) {
      printf ("wait for magic\n");
      sleep_100ms ();
    }
    printf ("magic1 and magic2 are ok\n");

    cur = START_1G;
    while (driver_stop == 0) {
again0:
      if (read_user_stop (gshare) != 0 && delay_stop++ > 2) {
        break;
      }

      bucket_info = (struct bucket_info_struct *) mmap ((void *) (((unsigned long) cur << 30) + 992 * CONST_1M), LEN_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

      end_cnt = 0;
      while (bucket_info->u.real.num_captured == 0) {
        sleep_100ms ();
        end_cnt++;
        if (read_user_stop (gshare) != 0 && end_cnt > 20) {
          if (munmap (bucket_info, LEN_BLOCK_SIZE) == -1) {
            perror ("munmap");
            return 1; 
          }
          goto again0;
        }
      }

      printf ("gets bucket#  0x%x\n", cur);
      packets = bucket_info->u.real.num_captured;
      total_packets += packets; 

      printf ("captured=%12ld  accum=%15ld\n", packets, total_packets);
      driver_stop = bucket_info->u.real.last;

      memset (&bucket_info->u.real, 0, sizeof (bucket_info->u.real));
      // bucket_info->u.real.max_row = MAX_4094;


      if (munmap (bucket_info, LEN_BLOCK_SIZE) == -1) {
        perror ("munmap");
        return 1; 
      }

      cur++;
      if (cur > END_1G)
        cur = START_1G;
    }

    if (munmap (gshare, 4096) == -1) {
        perror ("munmap");
        return 1; 
    }

    if (close (fd) == -1) {
        perror ("close");
        return 1; 
    }

    return 0;
} 



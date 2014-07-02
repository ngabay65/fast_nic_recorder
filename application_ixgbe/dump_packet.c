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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h> 

#include "../ixgbe-3.18.7/src/nlsim_ixgbe.h"

#define printk		printf

#define NUM_BUCKETS	(END_1G - START_1G + 1)

static struct bucket_info_struct *bucket_info = NULL;
static int num_writers, max_blocks;
static int num_rx_slots = 4096;

char writer_path [MAX_WRITERS][1024];

void read_disk_table (void)
{
  int i, num;
  FILE *fd;
  char buffer [1200], path [1024];

  for (i = 0; i < MAX_WRITERS; i++)
    writer_path [i][0] = '\0';

  fd = fopen ("writer_info.cfg", "r");
  if (fd == NULL) {
      printf ("Failed to open writer_info.cfg\n");
      exit (0);
  }
  if (fgets (buffer, 1199, fd) == NULL) {
    printf ("Read error, expect <num_writers> <max_blocks>\n");
    fclose (fd);
    exit (0);
  } 
  if (sscanf (buffer, "%d %d", &num_writers, &max_blocks) != 2) {
    printf ("Read error, expect <num_writers> <max_blocks>\n");
    fclose (fd);
    exit (0);
  }
  if (num_writers + 2 > (END_1G - START_1G + 1)) {
    printf ("<num_writers>=%d too many or <START_1G..END_1G>=%d too few\n", num_writers, END_1G - START_1G + 1);
    fclose (fd);
    exit (0);
  }
  if (max_blocks <= 0) {
    printf ("<max_blocks> must be positive\n");
    fclose (fd);
    exit (0);
  }
  for (i = 0; i < num_writers; i++) {
    if (fgets (buffer, 1199, fd) == NULL) {
      printf ("Read error, expect path for writer#%d\n", i);
      fclose (fd);
      exit (0);
    }
    if (sscanf (buffer, "%d %s", &num, path) != 2) {
      printf ("Expect <writer_num> <path of writer> for num=%d\n", i);
      fclose (fd);
      exit (0);
    }
    if (i != num) {
      printf ("<writer_num> is not in order, expect %d  found %d\n", i, num);
      fclose (fd);
      exit (0);
    }
    if (strlen (path) == 0 || strlen (path) > 1023) {
      printf ("Invalid path '%s' for writer_num=%d\n", path, num);
      fclose (fd);
      exit (0);
    }
    strcpy (writer_path [num], path);
  }
  fclose (fd);
}

void dump_one_packet (unsigned char *data, int len, long base, int cur_row, int cur_partial)
{
  int i;
  int offset = cur_row * num_rx_slots + cur_partial;

  printf ("packet# %12ld  len = %d ==>", base + offset, len);
  for (i = 0; i < len; i++) {
    if (i % 16 == 0) {
      printf ("\n[0x%03x]", i / 16);       
    }
    printf (" %02x", *data++);
  }  
  printf ("\n");
}

unsigned int local_len [32768];

void dump_packet (unsigned char *data, struct bucket_info_struct *bucket_info, long long base, long long start, long long count, long long *processed)
{
  int max_row, max_partial;
  int cur_row, cur_partial;
  int i, j;

  *processed = 0;

  max_row = bucket_info->u.real.num_captured / num_rx_slots;
  max_partial = bucket_info->u.real.num_captured % num_rx_slots;
  for (i = 0; i < num_rx_slots; i++)
    local_len [i] = 0;
  cur_row = start / num_rx_slots;
  cur_partial = start % num_rx_slots;
  for (i = 0; i < cur_row; i++) {
    for (j = 0; j < num_rx_slots; j++) {
      local_len [j] += bucket_info->len [i][j];
    }
  }
  for (j = 0; j < cur_partial; j++) {
    local_len [j] += bucket_info->len [cur_row][j];
  }
  while (1) {
    if (cur_row == max_row &&
        cur_partial == max_partial)
      return;
    if (count == 0)
      return;
    count--;
    (*processed)++;
    dump_one_packet (data + 240 * 1024 * cur_partial + local_len [cur_partial], bucket_info->len [cur_row][cur_partial], base, cur_row, cur_partial);
    local_len [cur_partial] += bucket_info->len [cur_row][cur_partial];
    cur_partial++;
    if (cur_partial == num_rx_slots) {
      cur_partial = 0;
      cur_row++;
    }
  }
}

int main (int argc, char *argv[])
{
    int fout;
    int file_count = 0, writer_num = 0;
    char fname [256];
    int fd;
    int cur;
    long long offs, start, count, processed, total_packets = 0;
    unsigned char *data_ptr;

    if (argc != 3) {
      printf ("dump_packet <start> <count>\n");
      return 0;
    }

    read_disk_table ();

    start = atol (argv [1]);
    if (start < 0) {
      printf ("invalid start=%lld\n", start);
      return 0;
    }

    count = atol (argv [2]);
    if (count < 0) {
      printf ("invalid count=%lld\n", count);
      return 0;
    }

    fd = open ("/dev/nlsim", O_RDWR);
    if (fd == -1) {
        perror ("open");
        return 1;
    }

    cur = START_1G;

    data_ptr = (unsigned char *) mmap ((void *) ((unsigned long) cur << 30), CONST_1G, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data_ptr == MAP_FAILED) {
        perror ("mmap");
        return 1; 
    }

    offs = 0;
    bucket_info = (struct bucket_info_struct *) (data_ptr + 992 * CONST_1M);
    while (1) {
      sprintf (fname, "%s/file%05d", writer_path [writer_num], file_count);
      writer_num = (writer_num + 1) % num_writers;
//      printf ("file=%s: ", fname);
      fout = open (fname, O_RDONLY, 0);
      if (fout < 0) {
        printf ("can not open store.dat\n");
        return 1;
      }
      read (fout, data_ptr + 992 * CONST_1M, 1 * CONST_1K);
      num_rx_slots = bucket_info->u.real.num_rx_slots;
      total_packets += bucket_info->u.real.num_captured;
      printf ("block#%05d  packets=%8d  accum=%12lld\n", file_count, bucket_info->u.real.num_captured, total_packets);
      file_count++;
      if (start >= bucket_info->u.real.num_captured) {
        offs += bucket_info->u.real.num_captured;
        start -= bucket_info->u.real.num_captured;
        continue;
      }
      lseek (fout, 0, SEEK_SET);
      read (fout, data_ptr + 992 * CONST_1M, LEN_BLOCK_SIZE);
      read (fout, data_ptr, 992 * CONST_1M);
      close (fout);
      dump_packet (data_ptr, bucket_info, offs, start, count, &processed);
      if (processed <= 0) {
        printf ("error: dumping packets\n");
        break;
      }
      if (count == processed) {
        printf ("done: dumping packets\n");
        break;
      }
      offs += bucket_info->u.real.num_captured;
      start =  start + processed - bucket_info->u.real.num_captured;
      count -= processed;
      processed = 0;
      if (bucket_info->u.real.stalled) {
        printf ("end of file code=stalled\n");
        break;
      }
      if (bucket_info->u.real.last) {
        printf ("end of file code=normal\n");
        break;
      }
    }
    if (munmap (data_ptr, CONST_1G) == -1) {
      perror ("munmap");
      return 1; 
    }

    if (close (fd) == -1) {
        perror ("close");
        return 1; 
    }

    return 0;
} 



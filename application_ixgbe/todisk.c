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
#include <sys/timeb.h>
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
static unsigned char *data_ptr;

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
      printf ("Read error, expect path for writer#%3d\n", i);
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
    return temp;
}

void sleep_50ms (void)
{
  struct timespec tim, delay;

  delay.tv_sec = 0;
  delay.tv_nsec = 50000000;

  nanosleep (&delay, &tim);
}

int main (int argc, char *argv[])
{
    int i, fd, fout, last, stalled, blocks_written = 0;
    int id, cur, end_cnt, file_count;
    int delay_stop = 0, display_throughput = 0, driver_stop = 0;
    struct timeb time1, time2;
    double delta;
    char fname [1024];

    if (argc < 2) {
      printf ("todisk <writer_ID> [debug]\n");
      return 0;
    }

    read_disk_table ();

    id = atoi (argv [1]);
    if (id < 0 || id >= num_writers) {
      printf ("writer_ID must be in [0,%d]\n", num_writers - 1);
      return 0;
    }

    if (argc > 2 && strcmp (argv [2], "debug") == 0) {
      display_throughput = 1;
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
      // printf ("wait for magic\n");
      sleep_50ms ();
    }
    // printf ("magic1 and magic2 are ok\n");

    stalled = 0;
    cur = START_1G + id;
    file_count = id;
    while (driver_stop == 0) {
again0:
      if (read_user_stop (gshare) != 0 && delay_stop++ > 2) {
        break;
      }

      bucket_info = (struct bucket_info_struct *) mmap ((void *) (((unsigned long) cur << 30) + 992 * CONST_1M), LEN_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (bucket_info == MAP_FAILED) {
        perror ("mmap");
        return 1; 
      }

      end_cnt = 0;
      while (bucket_info->u.real.num_captured == 0) {
        sleep_50ms ();
        end_cnt++;
        if (read_user_stop (gshare) != 0 && end_cnt > 40) {
          if (munmap (bucket_info, LEN_BLOCK_SIZE) == -1) {
            // perror ("munmap");
            return 1; 
          }
          goto again0;
        }
      }

      if (end_cnt == 0 && delay_stop == 0) {
        gshare->data [USER_ISSUE_STOP] = 1;
        printf ("writer#%3d detects disk stalling, terminates\n", id);
        bucket_info->u.real.stalled = stalled = 1;
      }

      // write len_block first (32 MBytes), then data (31x32 MBytes)
      sprintf (fname, "%s/file%05d", writer_path [id], file_count);
      fout = open (fname, O_WRONLY | O_CREAT, 0);
      if (fout < 0) {
        printf ("can not open file='%s'\n", fname);
        return 1;
      }
      ftime (&time1);
      // store timestamp
      time ((time_t *) bucket_info->u.real.user_stamp);
      write (fout, bucket_info, LEN_BLOCK_SIZE);
      last = bucket_info->u.real.last;
      bucket_info->u.real.num_captured = 0;
      if (munmap (bucket_info, LEN_BLOCK_SIZE) == -1) {
        perror ("munmap");
        return 1; 
      }

      // write 31 - 32 MBytes block
      for (i = 0; i < 31; i++) {
        data_ptr = (unsigned char *) mmap ((void *) (((unsigned long) cur << 30) + i * 32 * CONST_1M), 32 * CONST_1M, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data_ptr == MAP_FAILED) {
          perror ("mmap");
          return 1; 
        }
        write (fout, data_ptr, 32 * CONST_1M);
        if (munmap (data_ptr, 32 * CONST_1M) == -1) {
          perror ("munmap");
          return 1; 
        }
      }
      close (fout);
      ftime (&time2);
      delta = (double) time2.time + time2.millitm / 1000.0 -
                       time1.time - time1.millitm / 1000.0;
      if (display_throughput)
        printf ("writer#%3d  block#%8d  throughput = %lf MBytes/sec\n", id, file_count, (double) 1024.0 / delta);

      if (last || stalled) {
        printf ("writer#%3d writes last bucket#%d\n", id, file_count);
        break;
      }
      if (++blocks_written == max_blocks) 
        break;
      file_count += num_writers;
      cur = START_1G + ((cur - START_1G + num_writers) % NUM_BUCKETS);
    }

    printf ("writer#%3d terminates\n", id);
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



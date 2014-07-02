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

int main (int argc, char *argv[])
{
    time_t *stamp;
    int fout;
    int file_count, writer_num;
    char fname [256];
    int fd;
    int cur;
    unsigned char *data_ptr;

    if (argc != 2) {
      printf ("count_recorded <block_num>\n");
      return 0;
    }

    file_count = atol (argv [1]);
    if (file_count < 0) {
      printf ("invalid block number = %d\n", file_count);
      return 0;
    }

    read_disk_table ();

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

    bucket_info = (struct bucket_info_struct *) (data_ptr + 992 * CONST_1M);
    writer_num = file_count % num_writers;
    sprintf (fname, "%s/file%05d", writer_path [writer_num], file_count);
//  printf ("%s: ", fname);
    fout = open (fname, O_RDONLY, 0);
    if (fout < 0) {
      printf ("can not open %s\n", fname);
      return 1;
    }
    read (fout, data_ptr + 992 * CONST_1M, CONST_1K);
    stamp = (time_t *) bucket_info->u.real.user_stamp;
    printf ("block#%08d       time=%s", file_count, ctime (stamp));
    printf ("                     packets=%8d\n", bucket_info->u.real.num_captured);
    close (fout);
    if (bucket_info->u.real.stalled) {
      printf ("this block is the end of file code=stalled\n");
    }
    if (bucket_info->u.real.last) {
      printf ("this block is the end of file code=normal\n");
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



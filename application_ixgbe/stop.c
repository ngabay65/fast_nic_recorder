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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h> 

#include "../ixgbe-3.18.7/src/nlsim_ixgbe.h"

struct bucket_info_struct *bucket_info = NULL;

int is_magic_ok (struct global_share_struct *gshare)
{
  if (gshare->magic1 == MAGIC1 && gshare->magic2 == MAGIC2)
    return 1;
  return 0;
}

void issue_user_stop (struct global_share_struct *gshare)
{
    gshare->data [USER_ISSUE_STOP] = 1;
}

int main (int argc, char *argv[])
{
    int fd;

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
      sleep (1);
    }
    printf ("magic1 and magic2 are ok\n");

    // user_issue_stop 
    printf ("user_issue_stop\n");
    issue_user_stop (gshare);

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



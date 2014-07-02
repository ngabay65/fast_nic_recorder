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

int main (int argc, char *argv [])
{
  FILE *f_list, *f_val, *f_set;
  char buf1 [256], buf2 [256];
  int extract_mask, irq_mask, cur_single_mask, cur_double_mask;

  if (argc != 4) {
    printf ("make_irq_script <irq_list> <irq_value> <set_irqs>\n");
    return 0;
  }

  f_list = fopen (argv [1], "r");
  if (f_list == NULL) {
    printf ("Failed to open '%s' (file contains irqs file paths)\n", argv [1]);
    return 0;
  }

  f_val = fopen (argv [2], "r");
  if (f_val == NULL) {
    printf ("Failed to open '%s' (file contains irqs values)\n", argv [2]);
    fclose (f_list);
    return 0;
  }

  f_set = fopen (argv [3], "w");
  if (f_set == NULL) {
    printf ("Failed to open '%s' for output script file\n", argv [3]);
    fclose (f_list);
    fclose (f_val);
    return 0;
  }

  cur_single_mask = 0;
  cur_double_mask = 0;
  while (1) {
    if (fgets (buf1, 255, f_list) == NULL)
      break;
    if (fgets (buf2, 255, f_val) == NULL)
      break;
    sscanf (buf2, "%x", &irq_mask);

    // avoid mask 0x30, assume quad core
    extract_mask = irq_mask & 0x30;
    if (extract_mask == 0) {
      ;
    }
    else {
      irq_mask ^= extract_mask;
      irq_mask |= (extract_mask >> 2);
    }
    fprintf (f_set, "echo %x > %s", irq_mask, buf1);
  }

  fclose (f_list);
  fclose (f_val);
  fclose (f_set);
}


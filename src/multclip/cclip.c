/*
      poly_Boolean: a polygon clip library
      Copyright (C) 1997  Alexey Nikitin, Michael Leonov
      leonov@propro.iis.nsk.su

      This library is free software; you can redistribute it and/or
      modify it under the terms of the GNU Library General Public
      License as published by the Free Software Foundation; either
      version 2 of the License, or (at your option) any later version.

      This library is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
      Library General Public License for more details.

      You should have received a copy of the GNU Library General Public
      License along with this library; if not, write to the Free
      Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

      cclip.c
      (C) 1997 Michael Leonov

*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "polyarea.h"
#include "polyio.h"

int main( int argc, char *argv[] )
{
    POLYAREA *a = NULL, *b = NULL, *res = NULL;
    int op;
    char *s;

    if ( argc < 5 )
    {
        puts("Format: cclip opcode file1 file2 file3");
        puts("opcode - code of operation:");
        puts("AND for A & B");
        puts("SUB for A - B");
        puts("OR  for A | B");
        puts("XOR for A ^ B");
        puts("file1  - WLR file describing polygon A");
        puts("file2  - WLR file describing polygon B");
        puts("file3  - output");
        return -1;
    }

    for (s = argv[1]; *s; s++)
        *s = (char)toupper((int)*s);

    if ( !strcmp( argv[1], "AND") )
        op = PBO_ISECT;
    else if ( !strcmp( argv[1], "OR") )
        op = PBO_UNITE;
    else if ( !strcmp( argv[1], "XOR") )
        op = PBO_XOR;
    else if ( !strcmp( argv[1], "SUB") )
        op = PBO_SUB;
    else
    {
        puts("Invalid opcode, run CCLIP without parameters ");
        return -1;
    }

    if ( LoadPOLYAREA( &a, argv[2] ) != err_ok )
    {
        puts("Error while loading 1st polygon");
        poly_Free(&a);
        return -1;
    }

    if ( LoadPOLYAREA( &b, argv[3] ) != err_ok )
    {
        puts("Error while loading 2nd polygon");
        poly_Free(&a);
        poly_Free(&b);
        return -1;
    }

    if ( !(poly_Valid(a) && poly_Valid(b)) )
    {
        puts("One of polygons is not valid");
        poly_Free(&a);
        poly_Free(&b);
        return -1;
    }
    if (poly_Boolean(a, b, &res, op) != err_ok)
    {
        puts("Error while clipping");
        poly_Free(&a);
        poly_Free(&b);
        poly_Free(&res);
        return -1;
    }

    if (res != NULL)
        SavePOLYAREA(res, argv[4]);
    else
        puts("The resulting polygon is empty");
    poly_Free(&a);
    poly_Free(&b);
    poly_Free(&res);
    return 0;
}

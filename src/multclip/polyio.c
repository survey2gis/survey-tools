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

      polyio.c
      (C) 1997 Michael Leonov
      (C) 1997 Klamer Schutte (minor patches)
*/

#include <stdio.h>

#include "polyio.h"

static
PLINE *LoadPLINE(FILE * f)
{
    int cnt, i;
    PLINE *res = NULL;
    Vector v;

    fscanf(f, "%d", &cnt);
    if (cnt < 3) return NULL;
    for ( i = 0; i < cnt; i++ )
    {
        fscanf(f, "%lf %lf", v, v + 1);
        v[2] = (double)0.0;
        if ( res == NULL )
        {
            if ((res = poly_NewContour(v)) == NULL)
                return NULL;
        }
        else
            poly_InclVertex(res->head.prev, poly_CreateNode(v));
    }
    poly_PreContour(res, TRUE);
    return res;
}

int LoadPOLYAREA( POLYAREA **PA, char * fname)
{
    int cnt, pcnt, i, j;
    FILE *f = fopen( fname, "r" );
    PLINE *cntr;
    POLYAREA *p;

    *PA = NULL;
    if (f == NULL) return err_bad_parm;
    fscanf(f, "%d", &pcnt);
    if (pcnt < 1) return err_bad_parm;
    for (j = 0; j < pcnt; j++)
    {
        fscanf(f, "%d", &cnt);
        if (cnt < 1) return err_bad_parm;
        p = NULL;
        for ( i = 0; i < cnt; i++ )
        {
            if ( (cntr = LoadPLINE(f)) == NULL)
                return err_bad_parm;
            if ( (cntr->Flags & PLF_ORIENT) != (unsigned int)(i ? PLF_INV : PLF_DIR) )
                poly_InvContour(cntr);
            if (p == NULL)
                p = poly_Create();
            if (p == NULL)
                return err_no_memory;
            poly_InclContour(p, cntr);
            cntr = NULL;
            if (!poly_Valid(p))
                return err_bad_parm;
        }
        poly_M_Incl(PA, p);
    }
    fclose(f);
    return err_ok;
}

static
void SavePLINE(FILE * f, PLINE * res)
{
    VNODE *cur;

    fprintf(f, "%d\n", res->Count);
    cur = &res->head;
    do
    {
        fprintf(f, "%f %f\n", cur->point[0], cur->point[1] );
    } while ( ( cur = cur->next ) != &res->head );
}


int SavePOLYAREA( POLYAREA *PA, char * fname)
{
    int cnt;
    FILE *f = fopen( fname, "w" );
    PLINE *cntr;
    POLYAREA * curpa;

    if ( f == NULL ) return err_bad_parm;
    cnt = 0, curpa = PA; 
    do
    {
        cnt++;
    } while ( (curpa = curpa->f) != PA);

    fprintf(f, "%d\n", cnt);
    curpa = PA; 
    do
    {
        for ( cntr = curpa->contours, cnt = 0; cntr != NULL; cntr = cntr->next, cnt++ ) {}
        fprintf(f, "%d\n", cnt);
        for ( cntr = curpa->contours; cntr != NULL; cntr = cntr->next )
            SavePLINE(f, cntr);
    }  while ( (curpa = curpa->f) != PA);

    fclose(f);
    return err_ok;
}

/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	geom.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Memory structures for handling different types of survey geometries.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include "global.h"
#include "parser.h"


#ifndef GEOM_H
#define GEOM_H

#ifdef MAIN
/* set to "1" for storing coordinates as 2D */
unsigned short GEOM_2D_MODE;
#else
extern unsigned short GEOM_2D_MODE;
#endif

/*
 * List of geometry types.
 */
#define GEOM_TYPE_NONE		-1	/* unassigned */
#define GEOM_TYPE_POINT		0	/* simple point */
#define GEOM_TYPE_LINE		1	/* line or polyline */
#define GEOM_TYPE_POLY		2	/* polygon */
#define GEOM_TYPE_POINT_RAW	3	/* raw vertices */
#define GEOM_TYPE_ALL		4	/* all types of geometries */

/*
 * Names of geometry types.
 */
static const char GEOM_TYPE_NAMES[][10] =
{ "point", "line", "poly", "point_raw", "all", "" };

/* Used to compose output path for optional label layer */
#define GEOM_LABELS_SUFFIX	"labels"


/*
 * List of geometry types.
 */
#define GEOM_WINDING_REVERSE		-2 /* boundaries = ccw, holes = cw */
#define GEOM_WINDING_AUTO			-1 /* boundaries = cw, holes = ccw */
#define GEOM_WINDING_CW				0 /* clockwise */
#define GEOM_WINDING_CCW			1 /* counter clockwise */

/*
 * Intersection modes
 */
#define GEOM_INTERSECT_LINE_LINE	0 /* lines with lines */
#define GEOM_INTERSECT_LINE_POLY	1 /* lines with polygons */
#define GEOM_INTERSECT_POLY_POLY	2 /* polygons with polygons */

/*
 * Planarization modes
 */
#define GEOM_PLANAR_MODE_CONST		0 /* set to constant value */
#define GEOM_PLANAR_MODE_PLANE		1 /* project to plane equation*/
#define GEOM_PLANAR_MODE_FIRST		2 /* take from first vertex */
#define GEOM_PLANAR_MODE_FINAL		3 /* take from last vertex */

static const char GEOM_PLANAR_MODE_NAMES[][10] =
{ "const", "plane", "first", "final", "" };


#define GEOM_STORE_CHUNK_BIG		100 /* chunk size for memory allocations */
#define GEOM_STORE_CHUNK_SMALL		10 /* chunk size for memory allocations */

/* a geometry store contains in-memory representations of
 * points, lines and polygons in a format that is easy to process. */
typedef struct geom_store geom_store;
typedef struct geom_store_point geom_store_point;
typedef struct geom_store_line geom_store_line;
typedef struct geom_store_polygon geom_store_polygon;
/* a part is one line segment or ring of a more complex geometry */
typedef struct geom_part geom_part;
/* this stores intersections vertices for delayed geometry cleaning */
typedef struct geom_store_intersection geom_store_intersection;


/* A geometry store holds a hierarchical, strongly
 * structured collection of (multi-part) geometries.
 * It grows dynamically as more geometries are added.
 */
struct geom_store
{
	/* number of points, lines, etc. in store */
	unsigned int num_points;
	unsigned int num_points_raw;
	unsigned int num_lines;
	unsigned int num_polygons;
	/* pointers to the actual geometry data */
	geom_store_point *points;
	geom_store_point *points_raw;
	geom_store_line *lines;
	geom_store_polygon *polygons;
	/* intersection vertices */
	geom_store_intersection *lines_intersections;
	geom_store_intersection *polygons_intersections;
	/* free storage counters */
	unsigned int free_points;
	unsigned int free_points_raw;
	unsigned int free_lines;
	unsigned int free_polygons;
	/* paths and file names for data storage */
	char *path_points;
	char *path_points_atts;
	char *path_points_raw;
	char *path_points_raw_atts;
	char *path_lines;
	char *path_lines_atts;
	char *path_polys;
	char *path_polys_atts;
	char *path_all; /* for multi-geom capable output formats */
	char *path_all_atts;
	char *path_labels; /* for additional labels layer (2D points; optional) */
	char *path_labels_atts;
	char *path_labels_gva; /* need this for compatibility with gvSIG CE */
	/* min/max extents of geoms in this store */
	BOOLEAN min_x_set;
	BOOLEAN min_y_set;
	BOOLEAN min_z_set;
	BOOLEAN max_x_set;
	BOOLEAN max_y_set;
	BOOLEAN max_z_set;
	double min_x;
	double min_y;
	double min_z;
	double max_x;
	double max_y;
	double max_z;
	/* TRUE, if there is nothing stored */
	BOOLEAN is_empty;
};


/*
 * A simple point object to be stored in
 * a geometry store.
 */
struct geom_store_point
{
	unsigned int geom_id; /* unique ID */
	/* coordinate values */
	double X;
	double Y;
	double Z;
	/* copies of the attribute field contents (as strings) */
	char *atts[PRG_MAX_FIELDS];
	/* name of data source from which this geom was read */
	char *source;
	/* line in data source from which it was read */
	unsigned int line;
	/* TRUE if this is a 3D geom */
	BOOLEAN is_3D;
	/* TRUE if this geom slot is currently empty */
	BOOLEAN is_empty;
	/* TRUE, if this geometry is part of the current selection */
	BOOLEAN is_selected;
	/* 2D label point for this geometry (optional) */
	BOOLEAN has_label;
	double label_x;
	double label_y;

};


/* A simple or multi-part line geometry to be stored
 * in a geometry store. Most fields are equivalent to
 * those in struct "geom_store_point" (see above). */
struct geom_store_line
{
	unsigned int geom_id;
	unsigned int num_parts;
	geom_part *parts; /* number of parts. 1 or larger (multi-part geoms) */
	double length; /* total length of (multi-part) line */
	double bbox_x1, bbox_x2, bbox_y1, bbox_y2, bbox_z1, bbox_z2; /* bounding box */
	unsigned int free_parts; /* free slots for adding new parts */
	char *atts[PRG_MAX_FIELDS];
	char *source;
	unsigned int line;
	BOOLEAN is_3D;
	BOOLEAN is_empty;
	/* TRUE, if this geometry is part of the current selection */
	BOOLEAN is_selected;
};


/* A simple or multi-part polygon geometry to be stored
 * in a geometry store. Most fields are equivalent to
 * those in struct "geom_store_point" and "geom_store_line"(see above).
 * Complex polygons can also have inner rings (holes).
 * These will also be saved as simple parts with the field "hole" set
 * to "true" (see below). It is the job of the export
 * function to correctly model holes in the exported data
 * (e.g. shapelib does this automatically for Shapefile output). */
struct geom_store_polygon
{
	unsigned int geom_id;
	unsigned int num_parts;
	geom_part *parts;
	double length; /* in this case: circumference */
	double bbox_x1, bbox_x2, bbox_y1, bbox_y2, bbox_z1, bbox_z2; /* bounding box */
	unsigned int free_parts;
	char *atts[PRG_MAX_FIELDS];
	char *source;
	unsigned int line;
	BOOLEAN is_3D;
	BOOLEAN is_empty;
	/* TRUE, if this geometry is part of the current selection */
	BOOLEAN is_selected;
};


/* Contains an array of vertices for a part, i.e.
 * a segment of a line string or a ring of a complex
 * polygon. */
struct geom_part
{
	unsigned int num_vertices;
	double *X;
	double *Y;
	double *Z;
	/* 2D label point for this part (optional) */
	BOOLEAN has_label;
	double label_x;
	double label_y;
	BOOLEAN is_hole; /* for polygons: TRUE if this is a hole */
	BOOLEAN is_undershoot_first; /* for lines: TRUE if first vertex undershoots another line segment */
	double dist_undershoot_first;
	double x_undershoot_first;
	double y_undershoot_first;
	BOOLEAN is_undershoot_last; /* for lines: TRUE if last vertex undershoots another line segment */
	double dist_undershoot_last;
	double x_undershoot_last;
	double y_undershoot_last;
	BOOLEAN is_empty;
};


/* This stores all intersections along the path of
 * of one line or polygon geometry part. Two such
 * structs are attached to every 'geom_store': one
 * for keeping count of line intersections and one
 * for polygon intersections. */
struct geom_store_intersection
{
	unsigned int num_intersections;
	unsigned int capacity; /* for dynamically allocating more memory if needed */
	/* the following is kept for each intersection vertex */
	unsigned int *geom_id;
	unsigned int *part_id;
	double *X;
	double *Y;
	double *Z;
	int *v; /* index position at which to insert the intersection vertex */
	BOOLEAN *added; /* Has this been added to its associated geometry? */
};


/* multiplex raw data records into geometries with 1 to n vertices */
int geom_multiplex ( parser_data_store *storage, parser_desc *parser );

/* create new geometry store */
geom_store *geom_store_new ();

/* build geometries from raw vertices */
int geom_store_build ( geom_store *gs, parser_data_store **ds,
		parser_desc *parser, options *opts);

/* create output file paths */
int geom_store_make_paths ( geom_store *gs, options *opts, char *error );

/* destroy geometry store, releasing memory */
void geom_store_destroy ( geom_store *gs );

/* print geometry store content summary */
void geom_store_print 	( geom_store *gs, BOOLEAN print_points );

/* reset topological data structure */
void geom_topology_invalidate ( parser_data_store *ds, options *opts );

/* remove 3D vertices with duplicate coordinates */
int geom_topology_remove_duplicates ( parser_data_store *ds, options *opts, BOOLEAN in3D );

/* remove lines with less than 2 vertices */
int geom_topology_remove_splinters_lines ( parser_data_store *ds, options *opts );

/* remove polygons with less than 3 vertices */
int geom_topology_remove_splinters_polygons ( parser_data_store *ds, options *opts );

/* punch holes into overlapped areas of polygons */
unsigned int geom_topology_poly_overlap_2D ( geom_store *gs, parser_desc *parser );

/* snap polygon boundaries */
unsigned int geom_topology_snap_boundaries_2D ( geom_store *gs, options *opts );

/* detect nodes at line/boundary intersections */
unsigned int geom_topology_intersections_2D_detect ( geom_store *gs, options *opts, int mode,
		unsigned int*num_added, unsigned int *topo_errors );

/* remove overshoots and undershoots (dangles) from line geometries */
unsigned int geom_topology_clean_dangles_2D ( geom_store *gs, options *opts, unsigned int *topo_errors,
		unsigned int *num_detected, unsigned int *num_added );

/* sort vertices in polygons */
unsigned int geom_topology_sort_vertices ( geom_store *gs, int mode );

/* check if polygon part A lies completely within polygon part B */
BOOLEAN geom_tools_part_in_part_2D ( geom_part *A, geom_part *B );

/* ensure that all nodes of a geometry lie on the same plane */
void geom_tools_planarize ();

/* re-orientate data along synthetic local X-Y axes */
void geom_reorient_local_xz(parser_data_store *storage);

/* check if measurements have Z coordinates */
BOOLEAN geom_ds_has_z(parser_data_store *storage);

#endif /* GEOM_H */

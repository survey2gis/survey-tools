/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	parser.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Memory structures for defining an ASCII parser for geometries
 * 				and attributes.
 *
 * COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/

/*
 * NOTES
 *
 * This parser expects one record per line, each record with 0 or more fields.
 * Data that is not in this way (columns) has to be transformed before it can be parsed.
 *
 * Each field (column) must be fully defined (position on line, data type, etc.).
 *
 * Whitespace at the beginning of a field's content will be skipped.
 * Lines that are all whitespace or start with a comment sign will be skipped.
 *
 */

#ifndef PARSER_H
#define PARSER_H


#include "global.h"
#include "options.h"

/* maximum line length for parser schema files */
#define PARSER_MAX_FILE_LINE_LENGTH		5000

/* maximum number of distinct separators per field */
#define PARSER_MAX_SEPARATORS		16

/* maximum number of distinct comment marks */
#define PARSER_MAX_COMMENTS			16

/* parser section types (schema file) */
#define PARSER_SECTION_NONE			0
#define PARSER_SECTION_PARSER		1
#define PARSER_SECTION_FIELD		2

/* types of fields */
#define PARSER_FIELD_TYPE_UNDEFINED 	-1
#define PARSER_FIELD_TYPE_TEXT 			0
#define PARSER_FIELD_TYPE_INT 			1
#define PARSER_FIELD_TYPE_DOUBLE 		2

/* field types as they appear in the parser schema file */
static const char PARSER_FIELD_TYPE_NAMES[][10] =
{ "text", "integer", "double", "" };

/* equivalents of the above in KML export */
static const char PARSER_FIELD_TYPE_NAMES_KML[][10] =
{ "string", "int", "double", "" };

/* text field conversion modes */
#define PARSER_FIELD_CONVERT_NONE		0
#define PARSER_FIELD_CONVERT_UPPER		1
#define PARSER_FIELD_CONVERT_LOWER		2

static const char PARSER_FIELD_CONVERSIONS[][10] =
{ "none", "upper", "lower", "" };

/* lookup properties */
#define PARSER_LOOKUP_TAG		'@' /* prefix char for lookup pairs (text fields only) */
#define PARSER_LOOKUP_MAX		1000 /* max. number of lookup pairs per field */

/*
 * This software uses a very simple geometry types system.
 * There are only points, lines and polygons. The surveyor has to
 * make a decision which type to measure in the field.
 * Each type is represented by a "geometry tag" which is a
 * user-definable string that has to appear in a user-definable
 * field (the "tag field"). The tag field name must be specified
 * by the user. If it is not specified, then it is assumed to
 * be NULL, and all measurements in the input file(s) are interpreted
 * as single, unique point measurements.
 *
 * These are the different modes for geometry tagging.
 * Only one mode can be chosen per parser.
 *

 *    Mode "Min" (default mode):
 *    This mode keeps manual work in the field to a minimum.
 #
 *    For complex geometries, i.e. (poly)lines and polygons, the first
 *    vertex measurement of a geometry must be tagged with the corresponding
 *    geometry type. For points, this is optional by default.
 *    For geometry types that consist of more than one vertex, the first
 *    occurrence of the tag marks the start of the geometry.
 *    The end of the geometry is assumed when a new record with full
 *    attribute values is entered (see below).
 *    The attribute values for each geometry are only coded once,
 *    on the line that contains the first vertex. After that, the parser
 *    expects to encounter only coordinate fields and those set to be
 *    "persistent". Persistent fields will be parsed correctly, but their
 *    values will not be stored. Attribute values for the whole geometry
 *    will only be read from the first vertex' record.
 *    To _explicitely_ mark a measurement as the end/closing vertex for
 *    (poly)lines/polygons, take a measurement
 *    at a great distance. Just how great that distance should be can be
 *    defined as part of the parser description (0=disable). There is also
 *    _implicit_ closing of geometries: this happens if (a) the end of the
 *    current input file is reached, (b) a measurement with attribute values
 *    is encountered, but the attribute values are different from those
 *    of the first (opening) vertex.
 *
 *    By default, untagged geometries are read as simple points.
 *
 *    If the parser's "strict" property is set to TRUE, then a geometry tag
 *    must be provided for each first vertex, including points.
 *
 */
#define PARSER_TAG_MODE_MIN		0


/*
 *    Mode "Max":
 *    Assumes complete information about geometry and attributes
 *    in every record. More useful for import of geometry and attribute
 *    descriptions from databases etc. than for actual field surveys.
 *
 *    The geometry type is tagged at the end of each and every record.
 *    that describe all vertices of one geometry. The tag field must be a
 *    separate field that contains only the tag. Vertices that belong 
 *    together share a common value (CASE-SENSITIVE for text fields)
 *    in the key field ("key_field). Thus, in this case, a "key_field" MUST
 *    be set. Otherwise, the parser will not know how to group all vertices
 *    that constitute one geometry.
 *
 *    By default, untagged geometries are read as simple points.
 *    In strict mode, such records will be discarded.
 */
#define PARSER_TAG_MODE_MAX		1


/*
 * 	  Mode "End":
 * 	  This mode is compatible with Arch�oCAD. It can be used to process
 *    files in that format.
 *
 *    Geometries are tagged at the end of each of a set of measurements
 *    that describe all vertices of one geometry. The tag field must CONTAIN
 * 	  the tag character(s) as part of its content. But it does not have
 *    to store only the tag. Since only the first vertex' attributes are
 *    stored in the output file's attribute table, it is OK to store the
 *    tag as part of another field's value string. Vertices that belong
 *    together share a common value (CASE-SENSITIVE for text fields)
 *    in the key field ("key_field). Thus, in this case, a "key_field" MUST
 *    be set. Otherwise, the parser will not know how to group all vertices
 *    that constitute one geometry. The attribute table values will be read
 *    from the measurement immediately preceding the geometry-tagged record.
 *    Therefore, the tag will not appear in the attribute table, even if it
 *    is stored in the "key_field".
 *
 *    By default, untagged geometries (i.e. those with an empty tag field
 *    value, but not with a missing tag field!) are read as simple points.
 *    In strict mode, such records will be discarded.
 */
#define PARSER_TAG_MODE_END		2


/*
 * 	  Mode "None":
 * 	  This mode assumes that all measurements are simple point measurements.
 * 	  Useful for tasks such as surveying a digital terrain model.
 */
#define PARSER_TAG_MODE_NONE	3

static const char PARSER_MODE_NAMES[][5] =
{ "min", "max", "end", "none", "" };

/* default geometry tags */
#define PARSER_GEOM_TAG_POINT	"."
#define PARSER_GEOM_TAG_LINE	"-"
#define PARSER_GEOM_TAG_POLY	"+"

/* default data store memory chunk size */
#define PARSER_DATA_STORE_CHUNK	100

/* Forward declarations. See below for explanations */
typedef struct parser_field parser_field;
typedef struct parser_desc parser_desc;
typedef struct parser_record parser_record;
typedef struct parser_data_store parser_data_store;

/*
 * Description of a single field for the parser.
 * Each parser can either use field separators OR fixed field positions (and lengths).
 * In the case of string-separated fields, the fields will be parsed in order
 * of their definition in the schema file or database.
 *
 */
struct parser_field
{
	unsigned int definition; /* line of parser file after which field definition starts (for error messages) */
	char *name;	/* name to use for attribute field NOT CASE SENSITIVE, no longer than PARSER_MAX_FIELD_LEN */
	char *info; /* a string with some information about this field description; set to NULL if not used */
	short type; /* field type (see PARSER_FIELD_TYPE_ list above; default: "PARSER_FIELD_TYPE_UNDEFINED") */
	BOOLEAN empty_allowed; /* if set to "TRUE", then this field must have some content (default: "TRUE")
						if set to "FALSE", then the field can be empty. Empty numeric fields will be set to "0",
						empty text fields to the string "NULL".  Empty fields can be coded by keying in only
						the separator. */
	BOOLEAN empty_allowed_set;
	BOOLEAN unique;	/* 	set to TRUE if this field must have a unique (per geometry)
						value (CASE SENSITIVE for text; default: "FALSE") */
	BOOLEAN unique_set;
	BOOLEAN persistent; /* set to TRUE for fields that always exist, no matter if this is just a coordinate
						or a coordinates + attributes record (default: FALSE) */
	BOOLEAN persistent_set;
	BOOLEAN skip; /* always skip this field, never read its contents into attribute table (default: "FALSE") */
	BOOLEAN skip_set;
	short conversion_mode; /* for text fields: can be "upper", "lower" or "none" (default: "none") */
	BOOLEAN conversion_mode_set;
	char **separators; /* array of PARSER_MAX_SEPARATORS strings to mark end of field */
	BOOLEAN merge_separators; /* if set to "TRUE", adjacent field separators will be skipped */
	BOOLEAN merge_separators_set;
	BOOLEAN has_lookup; /* TRUE if this is a text field and its definition contains lookup pairs */
	char *lookup_old[PARSER_LOOKUP_MAX]; /* stores strings to be replaced */
	char *lookup_new[PARSER_LOOKUP_MAX]; /* stores strings to replace with */
	char quote; /* quotation mark character for text fields; set to "0" if not used */
	char *value; /* a constant field value (for pseudo fields) */
	BOOLEAN empty; /* set to "TRUE" if this is an empty field */
};


/*
 * A complete parser description.
 * Beside the structure of the input file (field definitions), the
 * parser also needs to know about how the different geometries are tagged.
 * Each supported geometry type (as defined in geom.h) is identified
 * by a user-definable tag string. There are some defaults (see geom.h), but
 * the user can override these (in schema files) if needed.
 */
struct parser_desc
{
	char *name; /* short name of parser or NULL */
	char *info; /* a string with some information about this parser description; set to NULL if not used */
	unsigned short tag_mode; /* see description of PARSER_TAG_MODE_ above (default: "PARSER_TAG_MODE_MIN") */
	/* an untagged record is interpreted as a simple point by default, but by setting 'tag_strict'
	   to "TRUE", untagged records will not be stored (default: "FALSE") */
	BOOLEAN tag_mode_set;
	BOOLEAN tag_strict;
	BOOLEAN tag_strict_set;
	BOOLEAN key_unique; /* TRUE if every geometry has a unique value in its key field (enabling multi-parts) */
	BOOLEAN key_unique_set;
	char **comment_marks; /* array of PARSER_MAX_COMMENTS strings that indicate commented lines */
	parser_field **fields; /* array of PRG_MAX_FIELDS field descriptions in parser, unused slots are NULL */
	char *tag_field; /* name of the one field that contains the geometry tags;
						may be identical with "key_field";
						if this field is NULL, then all measurements are taken to be single point measurements. */
	char *key_field; /* name of the field that contains the measurement key */
	int empty_val; /* an integer number that represents empty fields of any type */
	BOOLEAN empty_val_set; /* if FALSE, then empty numeric fields will be coded "0" and empty text
						   fields will be coded "" (default: FALSE) */
	/* names of the coordinate fields */
	char *coor_x;
	char *coor_y;
	char *coor_z;
	/* the following are the geometry tags for points, lines and polygons 
           they default to ".", "-" and "+" */
	char *geom_tag_point;
	char *geom_tag_line;
	char *geom_tag_poly;
	BOOLEAN empty; /* set to "TRUE" if this is an empty parser */
};


/*
 * This structure stores one (validated) record
 * from one of the input files, plus metadata.
 */
struct parser_record
{
	unsigned int line; /* the line of the input file/stdin this was read from */
	char **contents; /* the original field values, as read from input file */
	BOOLEAN *skip; /* TRUE for any field that can be missing in tag mode "min" */
	double x, y, z; /* the recorded coordinates (for quick access) */
	char *tag; /* geometry tag or NULL */
	char *key; /* key field or NULL */
	unsigned int geom_id; /* the ID of the geometry set this data belongs to */
	unsigned int part_id; /* consecutive part no. for multi-part geoms 0=main part */
	short int geom_type; /* geometry type (see geom.h), or -1 if unassigned */
	BOOLEAN written_out; /* TRUE if this record has been written and to output file */
	BOOLEAN is_valid; /* TRUE only if this record has been validated and is OK */
	BOOLEAN is_empty; /* TRUE if this is an empty record */
};


/*
 * This structure stores all records for one file
 * and keeps track of their number.
 * It also stores some additional information about the data store.
 * One data store is associated with exactly one input data source.
 */
struct parser_data_store
{
	char *input;	/* the name of the file this relates to; "-" for stdin */
	unsigned int slot; /* pointer to next free slot for storing a record */
	unsigned int num_records; /* total number in store, including empty records */
	unsigned int num_points;
	unsigned int num_lines;
	unsigned int num_polygons;
	unsigned int space_left; /* number of records for which there is still space */
	int num_fields; /* number of fields in each record (according to parser schema) */
	double offset_x, offset_y, offset_z; /* user-defined coordinate offsets */
	parser_record *records; /* array of stored records */
	/* the following can be NULL or the full paths of the output files produced
	 * from the data in this data store: */
	char *output_points;
	char *output_points_raw;
	char *output_lines;
	char *output_polygons;
};


/* call this to create a new and empty parser */
parser_desc *parser_desc_create ();

/* removes the entire parser object from memory, incl. field definitions */
void parser_desc_destroy ( parser_desc *parser );

/* read parser description (incl. all field descriptions) from ASCII file */
void parser_desc_set ( parser_desc *parser, options *opts );

/* validate parser description */
int parser_desc_validate ( parser_desc *parser, options *opts );

/* fuse records with same primary key and geom type into multi-part objects */
unsigned int parser_ds_fuse ( parser_data_store **storage, options *opts, parser_desc *parser );

/* validate entire data store for unique attributes */
unsigned int parser_ds_validate_unique ( parser_data_store **storage, options *opts, parser_desc *parser );

/* dump full parser and field descriptions */
void parser_dump ( parser_desc *parser, options *opts );

#ifdef GUI
void parser_dump_gui ( parser_desc *parser, options *opts );
#endif

/* create a new and empty field description */
parser_field *parser_field_create ();

/* creates a data store for one source of input data */
parser_data_store *parser_data_store_create ( const char *input, parser_desc *parser,  options *opts );

/* releases all memory associated with a data store */
void parser_data_store_destroy ( parser_data_store *ds );

/* main function that reads the input files and parses them,
 * storing the data in a data store for each input data source */
void parser_consume_input ( parser_desc *parser, options *opts, parser_data_store **storage );

#endif /* PARSER_H */

/***************************************************************************
 *
 * PROGRAM:	Survey2GIS
 * FILE:	reproj.h
 * AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
 * 				Landesamt fuer Denkmalpflege
 * 				http://www.denkmalpflege-bw.de/
 *
 * PURPOSE:	 	Provide helper functions and declarations for reprojection support
 *  		    using PROJ.4.
 *
 * COPYRIGHT:	(C) 2017 the authors
 *
 *		This program is free software under the GPL (>=v2)
 *		Read the file COPYING that comes with this software for details.
 ***************************************************************************/


#include <string.h>

#include "global.h"

#include "geom.h"
#include "options.h"


#ifndef REPROJ_H
#define REPROJ_H

#define REPROJ_PI	3.14159265358979323846 /* value of pi for deg<->rad conversions */

#define REPROJ_PROJ4_SEP			" +" /* separator sequence used in PROJ.4 SRS definition tokens */
#define REPROJ_PROJ4_TOKEN_TOWGS84	"towgs84=" /* token that represents datum transformation option */
#define REPROJ_PROJ4_TOKEN_NADGRIDS	"nadgrids=" /* token that represents grid transformation option */
#define REPROJ_PROJ4_TOKEN_NODEFS	"no_defs" /* prevents PROJ.4 from applying any default settings */

/**********************************************************
 * Index of all SRS shortcuts recognized by this software.
 **********************************************************/

/* local survey origin */
#define REPROJ_SHORTCUT_LOCAL	000
/* lat/lon coordinates with WGS84 datum */
#define REPROJ_SHORTCUT_WGS84	001
/* Google Mercator lat/lon */
#define REPROJ_SHORTCUT_GOOGLE	002
/* 60 UTM zones (N/S) with WGS84 datum */
#define REPROJ_SHORTCUT_UTM1N	003
#define REPROJ_SHORTCUT_UTM1S	004
#define REPROJ_SHORTCUT_UTM2N	005
#define REPROJ_SHORTCUT_UTM2S	006
#define REPROJ_SHORTCUT_UTM3N	007
#define REPROJ_SHORTCUT_UTM3S	008
#define REPROJ_SHORTCUT_UTM4N	009
#define REPROJ_SHORTCUT_UTM4S	010
#define REPROJ_SHORTCUT_UTM5N	011
#define REPROJ_SHORTCUT_UTM5S	012
#define REPROJ_SHORTCUT_UTM6N	013
#define REPROJ_SHORTCUT_UTM6S	014
#define REPROJ_SHORTCUT_UTM7N	015
#define REPROJ_SHORTCUT_UTM7S	016
#define REPROJ_SHORTCUT_UTM8N	017
#define REPROJ_SHORTCUT_UTM8S	018
#define REPROJ_SHORTCUT_UTM9N	019
#define REPROJ_SHORTCUT_UTM9S	020
#define REPROJ_SHORTCUT_UTM10N	021
#define REPROJ_SHORTCUT_UTM10S	022
#define REPROJ_SHORTCUT_UTM11N	023
#define REPROJ_SHORTCUT_UTM11S	024
#define REPROJ_SHORTCUT_UTM12N	025
#define REPROJ_SHORTCUT_UTM12S	026
#define REPROJ_SHORTCUT_UTM13N	027
#define REPROJ_SHORTCUT_UTM13S	028
#define REPROJ_SHORTCUT_UTM14N	029
#define REPROJ_SHORTCUT_UTM14S	030
#define REPROJ_SHORTCUT_UTM15N	031
#define REPROJ_SHORTCUT_UTM15S	032
#define REPROJ_SHORTCUT_UTM16N	033
#define REPROJ_SHORTCUT_UTM16S	034
#define REPROJ_SHORTCUT_UTM17N	035
#define REPROJ_SHORTCUT_UTM17S	036
#define REPROJ_SHORTCUT_UTM18N	037
#define REPROJ_SHORTCUT_UTM18S	038
#define REPROJ_SHORTCUT_UTM19N	039
#define REPROJ_SHORTCUT_UTM19S	040
#define REPROJ_SHORTCUT_UTM20N	041
#define REPROJ_SHORTCUT_UTM20S	042
#define REPROJ_SHORTCUT_UTM21N	043
#define REPROJ_SHORTCUT_UTM21S	044
#define REPROJ_SHORTCUT_UTM22N	045
#define REPROJ_SHORTCUT_UTM22S	046
#define REPROJ_SHORTCUT_UTM23N	047
#define REPROJ_SHORTCUT_UTM23S	048
#define REPROJ_SHORTCUT_UTM24N	049
#define REPROJ_SHORTCUT_UTM24S	050
#define REPROJ_SHORTCUT_UTM25N	051
#define REPROJ_SHORTCUT_UTM25S	052
#define REPROJ_SHORTCUT_UTM26N	053
#define REPROJ_SHORTCUT_UTM26S	054
#define REPROJ_SHORTCUT_UTM27N	055
#define REPROJ_SHORTCUT_UTM27S	056
#define REPROJ_SHORTCUT_UTM28N	057
#define REPROJ_SHORTCUT_UTM28S	058
#define REPROJ_SHORTCUT_UTM29N	059
#define REPROJ_SHORTCUT_UTM29S	060
#define REPROJ_SHORTCUT_UTM30N	061
#define REPROJ_SHORTCUT_UTM30S	062
#define REPROJ_SHORTCUT_UTM31N	063
#define REPROJ_SHORTCUT_UTM31S	064
#define REPROJ_SHORTCUT_UTM32N	065
#define REPROJ_SHORTCUT_UTM32S	066
#define REPROJ_SHORTCUT_UTM33N	067
#define REPROJ_SHORTCUT_UTM33S	068
#define REPROJ_SHORTCUT_UTM34N	069
#define REPROJ_SHORTCUT_UTM34S	070
#define REPROJ_SHORTCUT_UTM35N	071
#define REPROJ_SHORTCUT_UTM35S	072
#define REPROJ_SHORTCUT_UTM36N	073
#define REPROJ_SHORTCUT_UTM36S	074
#define REPROJ_SHORTCUT_UTM37N	075
#define REPROJ_SHORTCUT_UTM37S	076
#define REPROJ_SHORTCUT_UTM38N	077
#define REPROJ_SHORTCUT_UTM38S	078
#define REPROJ_SHORTCUT_UTM39N	079
#define REPROJ_SHORTCUT_UTM39S	080
#define REPROJ_SHORTCUT_UTM40N	081
#define REPROJ_SHORTCUT_UTM40S	082
#define REPROJ_SHORTCUT_UTM41N	083
#define REPROJ_SHORTCUT_UTM41S	084
#define REPROJ_SHORTCUT_UTM42N	085
#define REPROJ_SHORTCUT_UTM42S	086
#define REPROJ_SHORTCUT_UTM43N	087
#define REPROJ_SHORTCUT_UTM43S	088
#define REPROJ_SHORTCUT_UTM44N	089
#define REPROJ_SHORTCUT_UTM44S	090
#define REPROJ_SHORTCUT_UTM45N	091
#define REPROJ_SHORTCUT_UTM45S	092
#define REPROJ_SHORTCUT_UTM46N	093
#define REPROJ_SHORTCUT_UTM46S	094
#define REPROJ_SHORTCUT_UTM47N	095
#define REPROJ_SHORTCUT_UTM47S	096
#define REPROJ_SHORTCUT_UTM48N	097
#define REPROJ_SHORTCUT_UTM48S	098
#define REPROJ_SHORTCUT_UTM49N	099
#define REPROJ_SHORTCUT_UTM49S	100
#define REPROJ_SHORTCUT_UTM50N	101
#define REPROJ_SHORTCUT_UTM50S	102
#define REPROJ_SHORTCUT_UTM51N	103
#define REPROJ_SHORTCUT_UTM51S	104
#define REPROJ_SHORTCUT_UTM52N	105
#define REPROJ_SHORTCUT_UTM52S	106
#define REPROJ_SHORTCUT_UTM53N	107
#define REPROJ_SHORTCUT_UTM53S	108
#define REPROJ_SHORTCUT_UTM54N	109
#define REPROJ_SHORTCUT_UTM54S	110
#define REPROJ_SHORTCUT_UTM55N	111
#define REPROJ_SHORTCUT_UTM55S	112
#define REPROJ_SHORTCUT_UTM56N	113
#define REPROJ_SHORTCUT_UTM56S	114
#define REPROJ_SHORTCUT_UTM57N	115
#define REPROJ_SHORTCUT_UTM57S	116
#define REPROJ_SHORTCUT_UTM58N	117
#define REPROJ_SHORTCUT_UTM58S	118
#define REPROJ_SHORTCUT_UTM59N	119
#define REPROJ_SHORTCUT_UTM59S	120
#define REPROJ_SHORTCUT_UTM60N	121
#define REPROJ_SHORTCUT_UTM60S	122
/* DHDN (Germany) zones 2-5 */
#define REPROJ_SHORTCUT_DHDN2	123
#define REPROJ_SHORTCUT_DHDN3	124
#define REPROJ_SHORTCUT_DHDN4	125
#define REPROJ_SHORTCUT_DHDN5	126
/* OSGB (UK, Great Britain) */
#define REPROJ_SHORTCUT_OSGB	127

/* name of environment variable that will be checked for PROJ4 data folder path */
static const char REPROJ_PROJ4_ENVVAR[] = "PROJ_LIB";

/* name of (default, local) subdirectory with PROJ.4 data directories */
static const char REPROJ_PROJ4_DATA_DIR[] = "proj";

/* name of database file with PROJ.4 EPSG translations */
static const char REPROJ_PROJ4_EPSG_FILE[] = "epsg";

/****************************************************
 * List of SRS shortcuts recognized by this program.
 * All lists are terminated by an empty string.
 * All lists are concordant with each other and the
 * shortcut index above.
 ****************************************************/

/* maximum length (in chars) of an entry in the shortcuts list */
#define REPROJ_MAX_LEN_SHORTCUT_NAME		10

/* SRS shortcut list entries */
static const char REPROJ_SHORTCUT_NAME[][REPROJ_MAX_LEN_SHORTCUT_NAME] =
{ 	"local",
	"wgs84",
	"web",
	"utm1n",
	"utm2n",
	"utm3n",
	"utm4n",
	"utm5n",
	"utm6n",
	"utm7n",
	"utm8n",
	"utm9n",
	"utm10n",
	"utm11n",
	"utm12n",
	"utm13n",
	"utm14n",
	"utm15n",
	"utm16n",
	"utm17n",
	"utm18n",
	"utm19n",
	"utm20n",
	"utm21n",
	"utm22n",
	"utm23n",
	"utm24n",
	"utm25n",
	"utm26n",
	"utm27n",
	"utm28n",
	"utm29n",
	"utm30n",
	"utm31n",
	"utm32n",
	"utm33n",
	"utm34n",
	"utm35n",
	"utm36n",
	"utm37n",
	"utm38n",
	"utm39n",
	"utm40n",
	"utm41n",
	"utm42n",
	"utm43n",
	"utm44n",
	"utm45n",
	"utm46n",
	"utm47n",
	"utm48n",
	"utm49n",
	"utm50n",
	"utm51n",
	"utm52n",
	"utm53n",
	"utm54n",
	"utm55n",
	"utm56n",
	"utm57n",
	"utm58n",
	"utm59n",
	"utm60n",
	"utm1s",
	"utm2s",
	"utm3s",
	"utm4s",
	"utm5s",
	"utm6s",
	"utm7s",
	"utm8s",
	"utm9s",
	"utm10s",
	"utm11s",
	"utm12s",
	"utm13s",
	"utm14s",
	"utm15s",
	"utm16s",
	"utm17s",
	"utm18s",
	"utm19s",
	"utm20s",
	"utm21s",
	"utm22s",
	"utm23s",
	"utm24s",
	"utm25s",
	"utm26s",
	"utm27s",
	"utm28s",
	"utm29s",
	"utm30s",
	"utm31s",
	"utm32s",
	"utm33s",
	"utm34s",
	"utm35s",
	"utm36s",
	"utm37s",
	"utm38s",
	"utm39s",
	"utm40s",
	"utm41s",
	"utm42s",
	"utm43s",
	"utm44s",
	"utm45s",
	"utm46s",
	"utm47s",
	"utm48s",
	"utm49s",
	"utm50s",
	"utm51s",
	"utm52s",
	"utm53s",
	"utm54s",
	"utm55s",
	"utm56s",
	"utm57s",
	"utm58s",
	"utm59s",
	"utm60s",
	"dhdn2",
	"dhdn3",
	"dhdn4",
	"dhdn5",
	"osgb",
	"" };

/* maximum length (in chars) of an entry in the shortcuts descriptions list */
#define REPROJ_MAX_LEN_SHORTCUT_DESC		200

/* matching verbose descriptions of all SRS shortcuts */
static const char REPROJ_SHORTCUT_DESC[][REPROJ_MAX_LEN_SHORTCUT_DESC] =
{ 	"survey data with local origin",
	"geographic lat/lon data with WGS84 datum",
	"Web Mercator with lat/lon coordinates",
	"UTM X/Y coordinates (m) for zone 1, north of Equator (longitudes -180 to -174)",
	"UTM X/Y coordinates (m) for zone 2, north of Equator (longitudes -174 to -168)",
	"UTM X/Y coordinates (m) for zone 3, north of Equator (longitudes -168 to -162)",
	"UTM X/Y coordinates (m) for zone 4, north of Equator (longitudes -162 to -156)",
	"UTM X/Y coordinates (m) for zone 5, north of Equator (longitudes -156 to -150)",
	"UTM X/Y coordinates (m) for zone 6, north of Equator (longitudes -150 to -144)",
	"UTM X/Y coordinates (m) for zone 7, north of Equator (longitudes -144 to -138)",
	"UTM X/Y coordinates (m) for zone 8, north of Equator (longitudes -138 to -132)",
	"UTM X/Y coordinates (m) for zone 9, north of Equator (longitudes -132 to -126)",
	"UTM X/Y coordinates (m) for zone 10, north of Equator (longitudes -126 to -120)",
	"UTM X/Y coordinates (m) for zone 11, north of Equator (longitudes -120 to -114)",
	"UTM X/Y coordinates (m) for zone 12, north of Equator (longitudes -114 to -108)",
	"UTM X/Y coordinates (m) for zone 13, north of Equator (longitudes -108 to -102)",
	"UTM X/Y coordinates (m) for zone 14, north of Equator (longitudes -102 to -96)",
	"UTM X/Y coordinates (m) for zone 15, north of Equator (longitudes -96 to -90)",
	"UTM X/Y coordinates (m) for zone 16, north of Equator (longitudes -90 to -84)",
	"UTM X/Y coordinates (m) for zone 17, north of Equator (longitudes -84 to -78)",
	"UTM X/Y coordinates (m) for zone 18, north of Equator (longitudes -78 to -72)",
	"UTM X/Y coordinates (m) for zone 19, north of Equator (longitudes -72 to -66)",
	"UTM X/Y coordinates (m) for zone 20, north of Equator (longitudes -66 to -60)",
	"UTM X/Y coordinates (m) for zone 21, north of Equator (longitudes -60 to -54)",
	"UTM X/Y coordinates (m) for zone 22, north of Equator (longitudes -54 to -48)",
	"UTM X/Y coordinates (m) for zone 23, north of Equator (longitudes -48 to -42)",
	"UTM X/Y coordinates (m) for zone 24, north of Equator (longitudes -42 to -36)",
	"UTM X/Y coordinates (m) for zone 25, north of Equator (longitudes -36 to -30)",
	"UTM X/Y coordinates (m) for zone 26, north of Equator (longitudes -30 to -24)",
	"UTM X/Y coordinates (m) for zone 27, north of Equator (longitudes -24 to -18)",
	"UTM X/Y coordinates (m) for zone 28, north of Equator (longitudes -18 to -12)",
	"UTM X/Y coordinates (m) for zone 29, north of Equator (longitudes -12 to -6)",
	"UTM X/Y coordinates (m) for zone 30, north of Equator (longitudes -6 to 0)",
	"UTM X/Y coordinates (m) for zone 31, north of Equator (longitudes 0 to 6)",
	"UTM X/Y coordinates (m) for zone 32, north of Equator (longitudes 6 to 12)",
	"UTM X/Y coordinates (m) for zone 33, north of Equator (longitudes 12 to 18)",
	"UTM X/Y coordinates (m) for zone 34, north of Equator (longitudes 18 to 24)",
	"UTM X/Y coordinates (m) for zone 35, north of Equator (longitudes 24 to 30)",
	"UTM X/Y coordinates (m) for zone 36, north of Equator (longitudes 30 to 36)",
	"UTM X/Y coordinates (m) for zone 37, north of Equator (longitudes 36 to 42)",
	"UTM X/Y coordinates (m) for zone 38, north of Equator (longitudes 42 to 48)",
	"UTM X/Y coordinates (m) for zone 39, north of Equator (longitudes 48 to 54)",
	"UTM X/Y coordinates (m) for zone 40, north of Equator (longitudes 54 to 60)",
	"UTM X/Y coordinates (m) for zone 41, north of Equator (longitudes 60 to 66)",
	"UTM X/Y coordinates (m) for zone 42, north of Equator (longitudes 66 to 72)",
	"UTM X/Y coordinates (m) for zone 43, north of Equator (longitudes 72 to 78)",
	"UTM X/Y coordinates (m) for zone 44, north of Equator (longitudes 78 to 84)",
	"UTM X/Y coordinates (m) for zone 45, north of Equator (longitudes 84 to 90)",
	"UTM X/Y coordinates (m) for zone 46, north of Equator (longitudes 90 to 96)",
	"UTM X/Y coordinates (m) for zone 47, north of Equator (longitudes 96 to 102)",
	"UTM X/Y coordinates (m) for zone 48, north of Equator (longitudes 102 to 108)",
	"UTM X/Y coordinates (m) for zone 49, north of Equator (longitudes 108 to 114)",
	"UTM X/Y coordinates (m) for zone 50, north of Equator (longitudes 114 to 120)",
	"UTM X/Y coordinates (m) for zone 51, north of Equator (longitudes 120 to 126)",
	"UTM X/Y coordinates (m) for zone 52, north of Equator (longitudes 126 to 132)",
	"UTM X/Y coordinates (m) for zone 53, north of Equator (longitudes 132 to 138)",
	"UTM X/Y coordinates (m) for zone 54, north of Equator (longitudes 138 to 144)",
	"UTM X/Y coordinates (m) for zone 55, north of Equator (longitudes 144 to 150)",
	"UTM X/Y coordinates (m) for zone 56, north of Equator (longitudes 150 to 156)",
	"UTM X/Y coordinates (m) for zone 57, north of Equator (longitudes 156 to 162)",
	"UTM X/Y coordinates (m) for zone 58, north of Equator (longitudes 162 to 168)",
	"UTM X/Y coordinates (m) for zone 59, north of Equator (longitudes 168 to 174)",
	"UTM X/Y coordinates (m) for zone 60, north of Equator (longitudes 174 to 180)",
	"UTM X/Y coordinates (m) for zone 1, south of Equator (longitudes -180 to -174)",
	"UTM X/Y coordinates (m) for zone 2, south of Equator (longitudes -174 to -168)",
	"UTM X/Y coordinates (m) for zone 3, south of Equator (longitudes -168 to -162)",
	"UTM X/Y coordinates (m) for zone 4, south of Equator (longitudes -162 to -156)",
	"UTM X/Y coordinates (m) for zone 5, south of Equator (longitudes -156 to -150)",
	"UTM X/Y coordinates (m) for zone 6, south of Equator (longitudes -150 to -144)",
	"UTM X/Y coordinates (m) for zone 7, south of Equator (longitudes -144 to -138)",
	"UTM X/Y coordinates (m) for zone 8, south of Equator (longitudes -138 to -132)",
	"UTM X/Y coordinates (m) for zone 9, south of Equator (longitudes -132 to -126)",
	"UTM X/Y coordinates (m) for zone 10, south of Equator (longitudes -126 to -120)",
	"UTM X/Y coordinates (m) for zone 11, south of Equator (longitudes -120 to -114)",
	"UTM X/Y coordinates (m) for zone 12, south of Equator (longitudes -114 to -108)",
	"UTM X/Y coordinates (m) for zone 13, south of Equator (longitudes -108 to -102)",
	"UTM X/Y coordinates (m) for zone 14, south of Equator (longitudes -102 to -96)",
	"UTM X/Y coordinates (m) for zone 15, south of Equator (longitudes -96 to -90)",
	"UTM X/Y coordinates (m) for zone 16, south of Equator (longitudes -90 to -84)",
	"UTM X/Y coordinates (m) for zone 17, south of Equator (longitudes -84 to -78)",
	"UTM X/Y coordinates (m) for zone 18, south of Equator (longitudes -78 to -72)",
	"UTM X/Y coordinates (m) for zone 19, south of Equator (longitudes -72 to -66)",
	"UTM X/Y coordinates (m) for zone 20, south of Equator (longitudes -66 to -60)",
	"UTM X/Y coordinates (m) for zone 21, south of Equator (longitudes -60 to -54)",
	"UTM X/Y coordinates (m) for zone 22, south of Equator (longitudes -54 to -48)",
	"UTM X/Y coordinates (m) for zone 23, south of Equator (longitudes -48 to -42)",
	"UTM X/Y coordinates (m) for zone 24, south of Equator (longitudes -42 to -36)",
	"UTM X/Y coordinates (m) for zone 25, south of Equator (longitudes -36 to -30)",
	"UTM X/Y coordinates (m) for zone 26, south of Equator (longitudes -30 to -24)",
	"UTM X/Y coordinates (m) for zone 27, south of Equator (longitudes -24 to -18)",
	"UTM X/Y coordinates (m) for zone 28, south of Equator (longitudes -18 to -12)",
	"UTM X/Y coordinates (m) for zone 29, south of Equator (longitudes -12 to -6)",
	"UTM X/Y coordinates (m) for zone 30, south of Equator (longitudes -6 to 0)",
	"UTM X/Y coordinates (m) for zone 31, south of Equator (longitudes 0 to 6)",
	"UTM X/Y coordinates (m) for zone 32, south of Equator (longitudes 6 to 12)",
	"UTM X/Y coordinates (m) for zone 33, south of Equator (longitudes 12 to 18)",
	"UTM X/Y coordinates (m) for zone 34, south of Equator (longitudes 18 to 24)",
	"UTM X/Y coordinates (m) for zone 35, south of Equator (longitudes 24 to 30)",
	"UTM X/Y coordinates (m) for zone 36, south of Equator (longitudes 30 to 36)",
	"UTM X/Y coordinates (m) for zone 37, south of Equator (longitudes 36 to 42)",
	"UTM X/Y coordinates (m) for zone 38, south of Equator (longitudes 42 to 48)",
	"UTM X/Y coordinates (m) for zone 39, south of Equator (longitudes 48 to 54)",
	"UTM X/Y coordinates (m) for zone 40, south of Equator (longitudes 54 to 60)",
	"UTM X/Y coordinates (m) for zone 41, south of Equator (longitudes 60 to 66)",
	"UTM X/Y coordinates (m) for zone 42, south of Equator (longitudes 66 to 72)",
	"UTM X/Y coordinates (m) for zone 43, south of Equator (longitudes 72 to 78)",
	"UTM X/Y coordinates (m) for zone 44, south of Equator (longitudes 78 to 84)",
	"UTM X/Y coordinates (m) for zone 45, south of Equator (longitudes 84 to 90)",
	"UTM X/Y coordinates (m) for zone 46, south of Equator (longitudes 90 to 96)",
	"UTM X/Y coordinates (m) for zone 47, south of Equator (longitudes 96 to 102)",
	"UTM X/Y coordinates (m) for zone 48, south of Equator (longitudes 102 to 108)",
	"UTM X/Y coordinates (m) for zone 49, south of Equator (longitudes 108 to 114)",
	"UTM X/Y coordinates (m) for zone 50, south of Equator (longitudes 114 to 120)",
	"UTM X/Y coordinates (m) for zone 51, south of Equator (longitudes 120 to 126)",
	"UTM X/Y coordinates (m) for zone 52, south of Equator (longitudes 126 to 132)",
	"UTM X/Y coordinates (m) for zone 53, south of Equator (longitudes 132 to 138)",
	"UTM X/Y coordinates (m) for zone 54, south of Equator (longitudes 138 to 144)",
	"UTM X/Y coordinates (m) for zone 55, south of Equator (longitudes 144 to 150)",
	"UTM X/Y coordinates (m) for zone 56, south of Equator (longitudes 150 to 156)",
	"UTM X/Y coordinates (m) for zone 57, south of Equator (longitudes 156 to 162)",
	"UTM X/Y coordinates (m) for zone 58, south of Equator (longitudes 162 to 168)",
	"UTM X/Y coordinates (m) for zone 59, south of Equator (longitudes 168 to 174)",
	"UTM X/Y coordinates (m) for zone 60, south of Equator (longitudes 174 to 180)",
	"DHDN (Germany) 3-degree Gauss-Kruger zone 2",
	"DHDN (Germany) 3-degree Gauss-Kruger zone 3",
	"DHDN (Germany) 3-degree Gauss-Kruger zone 4",
	"DHDN (Germany) 3-degree Gauss-Kruger zone 5",
	"OSGB (Great Britain onshore; Isle of Man)",
	"" };

/* matching EPSG codes for SRS on the shortcut list */
static const int REPROJ_SHORTCUT_EPSG[] =
{	5806,	/* local survey data */
	4326,	/* wgs84 lat/lon */
	3857,	/* web mercator */
	32601,	/* utm1n */
	32602,	/* utm2n */
	32603,	/* utm3n */
	32604,	/* utm4n */
	32605,	/* utm5n */
	32606,	/* utm6n */
	32607,	/* utm7n */
	32608,	/* utm8n */
	32609,	/* utm9n */
	32610,	/* utm10n */
	32611,	/* utm11n */
	32612,	/* utm12n */
	32613,	/* utm13n */
	32614,	/* utm14n */
	32615,	/* utm15n */
	32616,	/* utm16n */
	32617,	/* utm17n */
	32618,	/* utm18n */
	32619,	/* utm19n */
	32610,	/* utm20n */
	32621,	/* utm21n */
	32622,	/* utm22n */
	32623,	/* utm23n */
	32624,	/* utm24n */
	32625,	/* utm25n */
	32626,	/* utm26n */
	32627,	/* utm27n */
	32628,	/* utm28n */
	32629,	/* utm29n */
	32630,	/* utm30n */
	32631,	/* utm31n */
	32632,	/* utm32n */
	32633,	/* utm33n */
	32634,	/* utm34n */
	32635,	/* utm35n */
	32636,	/* utm36n */
	32637,	/* utm37n */
	32638,	/* utm38n */
	32639,	/* utm39n */
	32640,	/* utm40n */
	32641,	/* utm41n */
	32642,	/* utm42n */
	32643,	/* utm43n */
	32644,	/* utm44n */
	32645,	/* utm45n */
	32646,	/* utm46n */
	32647,	/* utm47n */
	32648,	/* utm48n */
	32649,	/* utm49n */
	32650,	/* utm50n */
	32651,	/* utm51n */
	32652,	/* utm52n */
	32653,	/* utm53n */
	32654,	/* utm54n */
	32655,	/* utm55n */
	32656,	/* utm56n */
	32657,	/* utm57n */
	32658,	/* utm58n */
	32659,	/* utm59n */
	32660,	/* utm60n */
	32701,	/* utm1s */
	32702,	/* utm2s */
	32703,	/* utm3s */
	32704,	/* utm4s */
	32705,	/* utm5s */
	32706,	/* utm6s */
	32707,	/* utm7s */
	32708,	/* utm8s */
	32709,	/* utm9s */
	32710,	/* utm10s */
	32711,	/* utm11s */
	32712,	/* utm12s */
	32713,	/* utm13s */
	32714,	/* utm14s */
	32715,	/* utm15s */
	32716,	/* utm16s */
	32717,	/* utm17s */
	32718,	/* utm18s */
	32719,	/* utm19s */
	32710,	/* utm20s */
	32721,	/* utm21s */
	32722,	/* utm22s */
	32723,	/* utm23s */
	32724,	/* utm24s */
	32725,	/* utm25s */
	32726,	/* utm26s */
	32727,	/* utm27s */
	32728,	/* utm28s */
	32729,	/* utm29s */
	32730,	/* utm30s */
	32731,	/* utm31s */
	32732,	/* utm32s */
	32733,	/* utm33s */
	32734,	/* utm34s */
	32735,	/* utm35s */
	32736,	/* utm36s */
	32737,	/* utm37s */
	32738,	/* utm38s */
	32739,	/* utm39s */
	32740,	/* utm40s */
	32741,	/* utm41s */
	32742,	/* utm42s */
	32743,	/* utm43s */
	32744,	/* utm44s */
	32745,	/* utm45s */
	32746,	/* utm46s */
	32747,	/* utm47s */
	32748,	/* utm48s */
	32749,	/* utm49s */
	32750,	/* utm50s */
	32751,	/* utm51s */
	32752,	/* utm52s */
	32753,	/* utm53s */
	32754,	/* utm54s */
	32755,	/* utm55s */
	32756,	/* utm56s */
	32757,	/* utm57s */
	32758,	/* utm58s */
	32759,	/* utm59s */
	32760,	/* utm60s */
	31466,	/* dhdn2 */
	31467,	/* dhdn3 */
	31468,	/* dhdn4 */
	31469,	/* dhdn5 */
	27700,	/* osgb */
	-1 };

/* function return values */
/* reproj_need_reprojection() */
#define REPROJ_ACTION_ERROR		   	-1
#define REPROJ_ACTION_NONE			 0
#define REPROJ_ACTION_REPROJECT		 1

/* reproj_parse_opts() && reproj_do() */
#define REPROJ_STATUS_ERROR			-1
#define REPROJ_STATUS_NONE			 0
#define REPROJ_STATUS_OK			 1

/* initalizes reprojection system (call first!) */
void reproj_init( options *opt );

/* parse reprojection options */
int reproj_parse_opts( options *opts );

/* check if reprojection required/possible */
int reproj_need_reprojection ( options *opts );

/* perform PROJ.4 reprojection */
int reproj_do( options *opts, geom_store *gs );

/* checks if the input SRS is lat/lon */
BOOLEAN reproj_srs_in_latlon (options *opts);

/* checks if the output SRS is lat/lon */
BOOLEAN reproj_srs_out_latlon (options *opts);

#endif /* REPROJ_H */

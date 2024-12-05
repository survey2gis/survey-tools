#!/bin/awk -f

# TODO: Set EPSG DB version and record count from caller script.
# TODO: Handle empty/missing fields.
# TODO: Grep for "no data" values in CSV files.
# TODO: Add an array length variable.
# TODO: Add self-descriptions of types as string arrays(?).

# Set field separator as comma for CSV input and print C include file header.
BEGIN {
    FS=",";
	print "/* EPSG: Declarations of geographic coordinate systems (GDAL: gcs.csv) */";
	print "";
	print "typedef struct epsg_crs_gcs_type {";
	print "\t int coord_ref_sys_code;"				# COORD_REF_SYS_CODE
	print "\t const char* coord_ref_sys_name;"		# COORD_REF_SYS_NAME
	print "\t int datum_code;"						# DATUM_CODE
	print "\t const char* datum_name;"				# DATUM_NAME
	print "\t int greenwich_datum_code;"			# GREENWICH_DATUM
	print "\t int uom_code;"						# UOM_CODE
	print "\t int ellipsoid_code;"					# ELLIPSOID_CODE
	print "\t int prime_meridian_code;"
	print "\t BOOLEAN show_crs;"
	print "\t BOOLEAN deprecated;"
	print "\t int coord_sys_code;"
	print "\t int coord_op_code;"
	print "\t int coord_op_code_multi;"
	print "\t int coord_op_method_code;"
	print "\t double dx;"
	print "\t double dy;"
	print "\t double dz;"
	print "\t double rx;"
	print "\t double ry;"
	print "\t double rz;"
	print "\t double ds;"
	print "};";
	print "";
    print "epsg_crs_gcs_type EPSG_DATA_GCS[] = {";
}

# Produces one array row of data from the current CSV input line.
function printRow() {
	# "no data" representations
	NO_DATA_STR=""
	NO_DATA_INT="-9999"
	NO_DATA_DBL="-9999.99"

	# Attempt to read all fields. If a field has no data:
    # Store the "no data" representation for the field's type.

	# 1: COORD_REF_SYS_CODE (int)
	if (length($1) == 0) {
		COORD_REF_SYS_CODE=NO_DATA_INT;
	} else {
		COORD_REF_SYS_CODE=$1;
	}

	# 2: COORD_REF_SYS_NAME (str)
	if (length($2) == 0) {
		COORD_REF_SYS_NAME=NO_DATA_STR;
	} else {
		COORD_REF_SYS_NAME=$2;
	}

	# 3: DATUM_CODE (int)
	if (length($3) == 0) {
		DATUM_CODE=NO_DATA_INT;
	} else {
		DATUM_CODE=$3;
	}

	# 4: DATUM_NAME (str)
	if (length($4) == 0) {
		DATUM_NAME=NO_DATA_STR;
	} else {
		DATUM_NAME=$4;
	}

	# 5: GREENWICH_DATUM (int)
	if (length($5) == 0) {
		GREENWICH_DATUM=NO_DATA_INT;
	} else {
		GREENWICH_DATUM=$5;
	}

	# 6: UOM_CODE (int)
	if (length($6) == 0) {
		UOM_CODE=NO_DATA_INT;
	} else {
		UOM_CODE=$6;
	}

	#print "\t{ "COORD_REF_SYS_CODE", \""$2"\", "$3", \""$4"\", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14" },";
}

# If CSV file line number (NR variable) is 1, call printRow fucntion with 'th' as argument
#NR==1 {
#    printRow("th")
#}
# If CSV file line number (NR variable) is greater than 1, call printRow function (first row is header and will be skipped).
NR>1 {
    #printRow("td")
	printRow()
}

# Print include file footer.
END {
    print "};";
	print "";
}


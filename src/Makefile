#############################################################################
#
# PROGRAM:	survey2gis
# FILE:	Makefile
# AUTHOR(S):	Benjamin Ducke for Regierungspraesidium Stuttgart,
#				Landesamt fuer Denkmalpflege
# 				http://www.denkmalpflege-bw.de/
#
# PURPOSE:	 	This is the generic Makefile. It doesn't do anything.
#				Instead use "make -f Makefile.<system>".
#
# COPYRIGHT:	(C) 2011 by the gvSIG Community Edition team
#
#		This program is free software under the GPL (>=v2)
#		Read the file COPYING that comes with this software for details.
#############################################################################

PRG=survey2gis

# Bundled version of Shapelib
SHAPELIB_DIR=shapelib-1.3.0

# Bundled version of PROJ.4
PROJ4_DIR=proj-4.9.3

# Bundled version of SLRE
SLRE_DIR=slre

all:
	@echo "This Makefile does nothing."
	@echo "Please use 'make -f Makefile.<system>' to start compilation."
	@echo "Or use 'make clean' to clean non-source files for all platforms."

clean:
	@echo "Cleaning non-source files for all platforms."
	@echo "Just ignore any warnings about missing files."
# All
	rm -f *.o
	rm -f *.h.gch
	rm -f ${SHAPELIB_DIR}/*.o
	rm -f ${SHAPELIB_DIR}/libshp.a
	rm -f ${SLRE_DIR}/slre.o	
	rm -f ${PROJ4_DIR}/bin/*
	rm -rf ${PROJ4_DIR}/bin
	rm -rf ${PROJ4_DIR}/include
	rm -rf ${PROJ4_DIR}/lib/pkgconfig
	rm -rf ${PROJ4_DIR}/src/.deps
	rm -rf ${PROJ4_DIR}/src/.libs
	rm -f ${PROJ4_DIR}/lib/*
	rm -rf ${PROJ4_DIR}/lib
	rm -f ${PROJ4_DIR}/src/proj_config.h
	rm -f ${PROJ4_DIR}/src/*.o
	rm -f ${PROJ4_DIR}/src/*.lo
	rm -rf ${PROJ4_DIR}/share
	rm -f ${PROJ4_DIR}/config.log
	rm -f ${PROJ4_DIR}/config.status
	rm -f ${PROJ4_DIR}/libtool	
	rm -f ${PROJ4_DIR}/Makefile
	rm -f ${PROJ4_DIR}/proj.pc
	rm -f ${PROJ4_DIR}/cmake/Makefile
	rm -f ${PROJ4_DIR}/jniwrap/Makefile
	rm -f ${PROJ4_DIR}/jniwrap/org/Makefile
	rm -f ${PROJ4_DIR}/jniwrap/org/proj4/Makefile
	rm -f ${PROJ4_DIR}/man/Makefile
	rm -f ${PROJ4_DIR}/man/man1/Makefile
	rm -f ${PROJ4_DIR}/man/man3/Makefile
	rm -f ${PROJ4_DIR}/nad/Makefile
	rm -f ${PROJ4_DIR}/man/install
	rm -f ${PROJ4_DIR}/nad/install
	rm -f ${PROJ4_DIR}/src/stamp-h1
	rm -f ${PROJ4_DIR}/src/Makefile
	rm -f ${PROJ4_DIR}/src/libproj.la
	rm -rf proj
	rm -f config.h
# Linux/Mac OS X
	rm -f ${PRG}
	rm -f test-platform
	rm -f ${SHAPELIB_DIR}/shptest
	rm -f ${SHAPELIB_DIR}/shpcreate
	rm -f ${SHAPELIB_DIR}/shpadd
	rm -f ${SHAPELIB_DIR}/shpdump
	rm -f ${SHAPELIB_DIR}/shprewind
	rm -f ${SHAPELIB_DIR}/dbfcreate
	rm -f ${SHAPELIB_DIR}/dbfadd
	rm -f ${SHAPELIB_DIR}/dbfdump
	rm -f ${SHAPELIB_DIR}/sptreedump	
	rm -f ${PROJ4_DIR}/src/cs2cs
	rm -f ${PROJ4_DIR}/src/geod
	rm -f ${PROJ4_DIR}/src/nad2bin
	rm -f ${PROJ4_DIR}/src/proj	
# Windows
	rm -f ${PRG}.exe
	rm -f test-platform.exe
	rm -f ${SHAPELIB_DIR}/*.exe
	rm -f ${PROJ4_DIR}/src/cs2cs.exe
	rm -f ${PROJ4_DIR}/src/geod.exe
	rm -f ${PROJ4_DIR}/src/nad2bin.exe
	rm -f ${PROJ4_DIR}/src/proj.exe
	
install:
	@echo "Nothing to install here."
	@echo "Just run survey2gis from current directory."

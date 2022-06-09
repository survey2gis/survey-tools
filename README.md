# What is this?

Survey2GIS is a robust, powerful tool for processing survey data, such
as produced by a GPS or total station, and exporting it as topologically
cleaned data, fit for use in GIS. For more information, see
http://survey-tools.org and the user manual that ships with the software.

# Important links

Official Website: http://survey-tools.org.
Bugtracker: https://github.com/survey2gis/survey-tools/issues
Mailinglist: https://groups.io/g/survey2gis/topics

# FAQs

## How do I install this software?

Look into the folder "bin". In it, you will find subfolders
for the different operating systems that we support. Pick the
one that matches your OS. There is another "README" file in
that folder that has specific instructions for your OS.
Make sure to read it, as it may list some important 3rd
party software that you need to install before being able
to run Survey2GIS.

Also note that there are two versions of Survey2GIS for each OS.
The version in the OS folder itself contains a graphical user
interface (GUI. The subfolder "cli-only" contains a version
without a GUI that is more light-weight and suitable for use
from the command line/terminal window (CLI) only.

Just run "survey2gis[.exe]" to start the program.


## How do I use this software?

Just start the "survey2gis" executable and it will come up with a
graphical user interface. If you have chosen to run the CLI only
version (see above), then you can issue the following command
for a short summary of CLI syntax options:

```
 survey2gis -h
```

Detailed instructions can be found in the user manual that came with
this software.


## How do I compile this software from source code?

See the file "COMPILING" in the subfolder "src".


## What are the licensing terms?

This is free and open source software. For details, see the file
LICENSE that came with this software. If LICENSE is missing or
you would like to read a translation in another language, please
go to http://www.gnu.org/licenses/gpl.html.
Please note that Survey2GIS includes 3rd party software, which comes
with its own license agreement that you can find in corresponding
subdirectories.


## Who made this software?

The initial idea and funding for this software came from the
state heritage office in Baden-Wuerttemberg, Germany
(http://www.denkmalpflege-bw.de). Software development is
coordinated by CSGIS GbR (http://www.csgis.de). If you would
like to contribute, visit http://www.survey-tools.org and get
in touch!




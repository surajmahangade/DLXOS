# Application-specific makefile.  This file only needs to
# set the APPROOT, SRCS, HDRS, and EXEC variables properly 
# (i.e. the location of the apps directory in relation to this Makefile), and
# then include the Makerules file from the main apps directory.
# All the real work in done in Makerules.  Things are setup
# this way because the build procedure for all apps is basically the same.


SRCS=hello_world.c
EXEC=hello_world.dlx.obj
APPROOT=..
include ../Makerules

include $(APPROOT)/Makerules

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -D m -u makeprocs.dlx.obj hello_world.dlx.obj;




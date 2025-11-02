default:
	cd makeprocs; make
	cd hello_world; make
	cd count; make
	cd trap_access; make
	cd trap_address; make

clean:
	cd makeprocs; make clean
	cd hello_world; make clean
	cd count; make clean
	cd trap_access; make clean
	cd trap_address; make clean

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 5; ee469_fixterminal

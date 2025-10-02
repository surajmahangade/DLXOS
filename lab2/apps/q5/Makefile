default:
	cd makeprocs; make
	cd reaction1; make
	cd reaction2; make
	cd reaction3; make
	cd reaction4; make
	cd reaction5; make

clean:
	cd makeprocs; make clean
	cd reaction1; make clean
	cd reaction2; make clean
	cd reaction3; make clean
	cd reaction4; make clean
	cd reaction5; make clean

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 1 2; ee469_fixterminal

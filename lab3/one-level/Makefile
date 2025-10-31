q1:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/example && make clean'
	mainframer.sh 'cd apps/example && make'
	cd bin && dlxsim -D m -x os.dlx.obj -a -D m -u makeprocs.dlx.obj

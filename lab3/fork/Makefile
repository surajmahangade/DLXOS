q1:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/example && make clean'
	mainframer.sh 'cd apps/example && make'
	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 1 hello_world.dlx.obj'
# 	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 30 hello_world.dlx.obj'

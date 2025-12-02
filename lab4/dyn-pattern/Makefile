fdisk:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/fdisk && make clean'
	mainframer.sh 'cd apps/fdisk && make'
	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -D F -u fdisk.dlx.obj'
# 	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 30 hello_world.dlx.obj'

ostests:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/ostests && make clean'
	mainframer.sh 'cd apps/ostests && make'
	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -D F -u ostests.dlx.obj'

filetest:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/filetest && make clean'
	mainframer.sh 'cd apps/filetest && make'
	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -u filetest.dlx.obj'


pattern:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/pattern && make clean'
	mainframer.sh 'cd apps/pattern && make'
	mainframer.sh 'cd bin && dlxsim -x os.dlx.obj -a -u pattern.dlx.obj'

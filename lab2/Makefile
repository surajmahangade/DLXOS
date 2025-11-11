NumProcesses ?= 4
num_N3 ?= 1
num_H2O ?= 2
q2:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/q2 && make clean'
	mainframer.sh 'cd apps/q2 && make'
	mainframer.sh 'cd apps/q2 && make run NumProcesses=$(NumProcesses)'

q3:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/q3 && make clean'
	mainframer.sh 'cd apps/q3 && make'
	mainframer.sh 'cd apps/q3 && make run NumProcesses=$(NumProcesses)'

q4:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/q4 && make clean'
	mainframer.sh 'cd apps/q4 && make'
	mainframer.sh 'cd apps/q4 && make run NumProcesses=$(NumProcesses)'

q5:
	mainframer.sh 'cd os && make clean'
	mainframer.sh 'cd os && make'
	mainframer.sh 'cd apps/q5 && make clean'
	mainframer.sh 'cd apps/q5 && make'
	mainframer.sh 'cd apps/q5 && make run num_N3=$(num_N3) num_H2O=$(num_H2O)'

clean:
	mainframer.sh 'cd apps/q2 && make clean'
	mainframer.sh 'cd apps/q3 && make clean'
	mainframer.sh 'cd apps/q4 && make clean'
	mainframer.sh 'cd os && make clean'
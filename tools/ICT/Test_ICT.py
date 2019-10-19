import os, sys, shutil, subprocess, re

def filter_lines(file, pattern):
	out = ""
	regexp = re.compile(pattern)
	with open(file, "r") as f:
		for line in f:
			if (re.search(regexp, line)):
				out += line
	return out

def shell(cmd, cwd):
	print(cmd)
	cmd_seq = cmd.split('|')
	#~ print(cmd_seq)
	input_pipe = None
	process = None
	for c in cmd_seq:
		shell_cmd = c.split()
		#print(shell_cmd, cwd)
		process = subprocess.Popen(shell_cmd, cwd=cwd, stdin = input_pipe, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=False)
		if input_pipe:
			input_pipe.close()
		input_pipe = process.stdout
		
	if process:
		process.communicate()

def mkdir(dir):
	if not os.path.exists(dir):
		os.makedirs(dir)

def rmdir(dir):
	if os.path.exists(dir):
		shutil.rmtree(dir)

def copy(src, dst):
	if os.path.isdir(src):
		shutil.copytree(src, dst)
	else:
		shutil.copyfile(src, dst)		

#TODO add build commands
#TODO add multiple reports
#TODO add logs check
#TODO add release check

mkpath = lambda path: os.path.realpath(os.path.join(sys.path[0], path))

rmdir(mkpath('../../build/Test_ICT'))
mkdir(mkpath('../../build/Test_ICT'))
copy(mkpath('../../data/ICT/ref_win'), mkpath('../../build/Test_ICT/ref'))
copy(mkpath('../../data/ICT/Report.html'), mkpath('../../build/Test_ICT/Report.html'))
copy(mkpath('../../data/ICT/test_result_win.report'), mkpath('../../build/Test_ICT/test_result.report'))
mkdir(mkpath('../../build/Test_ICT/x86'))
mkdir(mkpath('../../build/Test_ICT/x64'))
shell(mkpath('../../build/Test_ICT_Debug_Win32/Test_ICT.exe'), mkpath('../../build/Test_ICT/x86'))
shell(mkpath('../../build/Test_ICT_Debug_x64/Test_ICT.exe'), mkpath('../../build/Test_ICT/x64'))
print(filter_lines(mkpath('../../build/Test_ICT/x86/Test_ICT.exe.log'), "( ERR\|)"))
print(filter_lines(mkpath('../../build/Test_ICT/x64/Test_ICT.exe.log'), "( ERR\|)"))
os.system(mkpath('../../build/Test_ICT/Report.html'))

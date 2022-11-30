# Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
# Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
#
# This file contains modified code from the REI project source code
# (see https://github.com/Vi3LM/REI).

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

msbuildPath = r'"' + os.environ.get("PROGRAMFILES") + r'\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\msbuild.exe" '
        
def build_project(projectPath, configuration, platform):
    print(msbuildPath + projectPath + " -verbosity:minimal -property:Configuration="+ configuration + " -property:Platform=" + platform)
    os.system(msbuildPath + projectPath + " -verbosity:minimal -property:Configuration="+ configuration + " -property:Platform=" + platform)

def create_combined_report(outReportPath, reportPaths=[]):
    outFile = open(outReportPath, 'w')
    test_results = "test_results = ["
    for reportPath in reportPaths:
        reportFile = open(reportPath,'r')
        test_results = ''.join((test_results,reportFile.read(),', '))
        reportFile.close()
    
    outFile.write(''.join((test_results,'];')))
    outFile.close()
        


#TODO add multiple reports
#TODO add logs check
#TODO add release check

mkpath = lambda path: os.path.realpath(os.path.join(sys.path[0], path))
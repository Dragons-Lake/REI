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

from Test_ICT_Include import *
sys.path.append("../../sources/REI_Platforms/windows/ICT")
import Test_ICT_Windows

rmdir(mkpath('../../build/Test_ICT'))
mkdir(mkpath('../../build/Test_ICT'))
copy(mkpath('../../data/ICT_ref_images'), mkpath('../../build/Test_ICT/ICT_ref_images'))
copy(mkpath('./ICTReport.html'), mkpath('../../build/Test_ICT/ICTReport.html'))

reportPaths = [mkpath('../../sources/REI_Platforms/windows/ICT/test_result_windows.report')]

if os.path.isfile("../../sources/REI_Platforms/xbox/ICT/test_result_xbox.report"):
    reportPaths.append(mkpath("../../sources/REI_Platforms/xbox/ICT/test_result_xbox.report"))
else:
    print("Warning: Path '../../sources/REI_Platforms/xbox/ICT/test_result_xbox.report' doesn't exist and xbox won't be included in report")

if os.path.isfile("../../sources/REI_Platforms/switch/ICT/test_result_switch.report"):
    reportPaths.append(mkpath("../../sources/REI_Platforms/switch/ICT/test_result_switch.report"))
else:
    print("Warning: Path '../../sources/REI_Platforms/switch/ICT/test_result_switch.report' doesn't exist and switch won't be included in report")

if os.path.isfile("../../sources/REI_Platforms/ps4/ICT/test_result_ps4.report"):
    reportPaths.append(mkpath("../../sources/REI_Platforms/ps4/ICT/test_result_ps4.report"))
else:
    print("Warning: Path '../../sources/REI_Platforms/ps4/ICT/test_result_ps4.report' doesn't exist and ps4 won't be included in report")

if os.path.isfile("../../sources/REI_Platforms/ps5/ICT/test_result_ps5.report"):
    reportPaths.append(mkpath("../../sources/REI_Platforms/ps5/ICT/test_result_ps5.report"))
else:
    print("Warning: Path '../../sources/REI_Platforms/ps5/ICT/test_result_ps5.report' doesn't exist and ps5 won't be included in report")

create_combined_report(mkpath('../../build/Test_ICT/test_result.report'), reportPaths)

Test_ICT_Windows.build_tests()
Test_ICT_Windows.run_tests()

if os.path.isfile("../../sources/REI_Platforms/xbox/ICT/Test_ICT_Xbox.py"):
    sys.path.append("../../sources/REI_Platforms/xbox/ICT")
    try:
        import Test_ICT_Xbox
        Test_ICT_Xbox.build_tests()
        Test_ICT_Xbox.run_tests()
    except:
        print("Error occurred")
else:
    print("Warning: Path '../../sources/REI_Platforms/xbox/ICT/Test_ICT_Xbox.py' doesn't exist")

        
if os.path.isfile("../../sources/REI_Platforms/switch/ICT/Test_ICT_Switch.py"):
    sys.path.append("../../sources/REI_Platforms/switch/ICT")
    try:
        import Test_ICT_Switch
        Test_ICT_Switch.build_tests()
        Test_ICT_Switch.run_tests()
    except:
        print("Error occurred")
else:
    print("Warning: Path '../../sources/REI_Platforms/switch/ICT/Test_ICT_Switch.py' doesn't exist")

if os.path.isfile("../../sources/REI_Platforms/ps4/ICT/Test_ICT_PS4.py"):
    sys.path.append("../../sources/REI_Platforms/ps4/ICT")
    try:
        import Test_ICT_PS4
        Test_ICT_PS4.build_tests()
        Test_ICT_PS4.run_tests()
    except:
        print("Error occurred")
else:
    print("Warning: Path '../../sources/REI_Platforms/ps4/ICT/Test_ICT_PS4.py' doesn't exist")

if os.path.isfile("../../sources/REI_Platforms/ps5/ICT/Test_ICT_PS5.py"):
    sys.path.append("../../sources/REI_Platforms/ps5/ICT")
    try:
        import Test_ICT_PS5
        Test_ICT_PS5.build_tests()
        Test_ICT_PS5.run_tests()
    except:
        print("Error occurred")
else:
    print("Warning: Path '../../sources/REI_Platforms/ps5/ICT/Test_ICT_PS5.py' doesn't exist")

os.system(mkpath('../../build/Test_ICT/ICTReport.html'))

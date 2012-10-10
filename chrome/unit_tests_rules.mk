OutDirCygwin:=$(shell cygpath -u "$(OutDir)")
IntDirCygwin:=$(shell cygpath -u "$(IntDir)")
all: $(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\data\measurement_system.js $(OutDirCygwin)\test_data\chrome\browser\resources\shared\js\cr.js $(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\print_preview_utils.js $(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\data\measurement_system_unittest.gtestjs $(IntDirCygwin)\chrome\browser\resources\print_preview\print_preview_utils_unittest-gen.cc $(IntDirCygwin)\chrome\test\data\unit\framework_unittest-gen.cc
	mkdir -p `cygpath -u "$(OutDir)\test_data\chrome\browser\resources\print_preview\data"`
	mkdir -p `cygpath -u "$(IntDir)\chrome\browser\resources\print_preview\data"`
	mkdir -p `cygpath -u "$(IntDir)\chrome\test\data\unit"`
	mkdir -p `cygpath -u "$(IntDir)\chrome\browser\resources\print_preview"`
	mkdir -p `cygpath -u "$(OutDir)\test_data\chrome\browser\resources\shared\js"`
	mkdir -p `cygpath -u "$(OutDir)\test_data\chrome\test\data\unit"`
	mkdir -p `cygpath -u "$(OutDir)\test_data\chrome\browser\resources\print_preview"`

$(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\data\measurement_system.js: browser\resources\print_preview\data\measurement_system.js ..\build\cp.py
	"python" "../build/cp.py" "browser\resources\print_preview\data\measurement_system.js" "$(OutDir)/test_data/chrome/browser\resources\print_preview\data/measurement_system.js"

$(OutDirCygwin)\test_data\chrome\browser\resources\shared\js\cr.js: browser\resources\shared\js\cr.js ..\build\cp.py
	"python" "../build/cp.py" "browser\resources\shared\js\cr.js" "$(OutDir)/test_data/chrome/browser\resources\shared\js/cr.js"

$(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\print_preview_utils.js: ..\build\cp.py browser\resources\print_preview\print_preview_utils.js
	"python" "../build/cp.py" "browser\resources\print_preview\print_preview_utils.js" "$(OutDir)/test_data/chrome/browser\resources\print_preview/print_preview_utils.js"

$(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\data\measurement_system_unittest.gtestjs $(IntDirCygwin)\chrome\browser\resources\print_preview\data\measurement_system_unittest-gen.cc: browser\resources\print_preview\data\measurement_system_unittest.gtestjs $(OutDirCygwin)\v8_shell.exe ..\chrome\test\base\js2gtest.js ..\chrome\test\data\webui\test_api.js ..\tools\gypv8sh.py ..\chrome\third_party\mock4js\mock4js.js
	"python" "../tools/gypv8sh.py" "$(OutDir)/v8_shell.exe" "../chrome/third_party/mock4js/mock4js.js" "../chrome/test/data/webui/test_api.js" "../chrome/test/base/js2gtest.js" "unit" "browser\resources\print_preview\data\measurement_system_unittest.gtestjs" "chrome/browser\resources\print_preview\data/measurement_system_unittest.gtestjs" "$(IntDir)/chrome/browser\resources\print_preview\data/measurement_system_unittest-gen.cc" "$(OutDir)/test_data/chrome/browser\resources\print_preview\data/measurement_system_unittest.gtestjs"

$(IntDirCygwin)\chrome\browser\resources\print_preview\print_preview_utils_unittest-gen.cc $(OutDirCygwin)\test_data\chrome\browser\resources\print_preview\print_preview_utils_unittest.gtestjs: $(OutDirCygwin)\v8_shell.exe ..\chrome\test\base\js2gtest.js ..\chrome\test\data\webui\test_api.js browser\resources\print_preview\print_preview_utils_unittest.gtestjs ..\tools\gypv8sh.py ..\chrome\third_party\mock4js\mock4js.js
	"python" "../tools/gypv8sh.py" "$(OutDir)/v8_shell.exe" "../chrome/third_party/mock4js/mock4js.js" "../chrome/test/data/webui/test_api.js" "../chrome/test/base/js2gtest.js" "unit" "browser\resources\print_preview\print_preview_utils_unittest.gtestjs" "chrome/browser\resources\print_preview/print_preview_utils_unittest.gtestjs" "$(IntDir)/chrome/browser\resources\print_preview/print_preview_utils_unittest-gen.cc" "$(OutDir)/test_data/chrome/browser\resources\print_preview/print_preview_utils_unittest.gtestjs"

$(IntDirCygwin)\chrome\test\data\unit\framework_unittest-gen.cc $(OutDirCygwin)\test_data\chrome\test\data\unit\framework_unittest.gtestjs: test\data\unit\framework_unittest.gtestjs $(OutDirCygwin)\v8_shell.exe ..\chrome\test\base\js2gtest.js ..\chrome\test\data\webui\test_api.js ..\tools\gypv8sh.py ..\chrome\third_party\mock4js\mock4js.js
	"python" "../tools/gypv8sh.py" "$(OutDir)/v8_shell.exe" "../chrome/third_party/mock4js/mock4js.js" "../chrome/test/data/webui/test_api.js" "../chrome/test/base/js2gtest.js" "unit" "test\data\unit\framework_unittest.gtestjs" "chrome/test\data\unit/framework_unittest.gtestjs" "$(IntDir)/chrome/test\data\unit/framework_unittest-gen.cc" "$(OutDir)/test_data/chrome/test\data\unit/framework_unittest.gtestjs"


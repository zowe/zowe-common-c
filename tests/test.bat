@echo off

SET WORKING_DIR=%undefined%
echo "********************************************************************************"
echo "Running Test"
SET date_stamp=%undefined%
SET COMMON=%CD%.\%CD%.
SET QUICKJS="%COMMON%\deps\configmgr\quickjs"
SET QUICKJS_LOCATION=git@github.com:joenemo\quickjs-portable.git
SET QUICKJS_BRANCH=main
SET LIBYAML="%COMMON%\deps\configmgr\libyaml"
SET LIBYAML_LOCATION=git@github.com:yaml\libyaml.git
SET LIBYAML_BRANCH=0.2.5
SET DEPS=QUICKJS LIBYAML
SET IFS= 
REM UNKNOWN: {"type":"For","name":{"text":"dep","type":"Name"},"wordlist":[{"text":"${DEPS}","expansion":[{"loc":{"start":0,"end":6},"parameter":"DEPS","type":"ParameterExpansion"}],"type":"Word"}],"do":{"type":"CompoundList","commands":[{"type":"Command","name":{"text":"eval","type":"Word"},"suffix":[{"text":"directory=\"\\$${dep}\"","expansion":[{"loc":{"start":13,"end":18},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"}]},{"type":"Command","name":{"text":"echo","type":"Word"},"suffix":[{"text":"\"Check if dir exist=$directory\"","expansion":[{"loc":{"start":20,"end":29},"parameter":"directory","type":"ParameterExpansion"}],"type":"Word"}]},{"type":"If","clause":{"type":"CompoundList","commands":[{"type":"Command","name":{"text":"[","type":"Word"},"suffix":[{"text":"!","type":"Word"},{"text":"-d","type":"Word"},{"text":"\"$directory\"","expansion":[{"loc":{"start":1,"end":10},"parameter":"directory","type":"ParameterExpansion"}],"type":"Word"},{"text":"]","type":"Word"}]}]},"then":{"type":"CompoundList","commands":[{"type":"Command","name":{"text":"eval","type":"Word"},"suffix":[{"text":"echo","type":"Word"},{"text":"Clone:","type":"Word"},{"text":"\\$${dep}_LOCATION","expansion":[{"loc":{"start":2,"end":7},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"},{"text":"@","type":"Word"},{"text":"\\$${dep}_BRANCH","expansion":[{"loc":{"start":2,"end":7},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"},{"text":"to","type":"Word"},{"text":"\\$${dep}","expansion":[{"loc":{"start":2,"end":7},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"}]},{"type":"Command","name":{"text":"eval","type":"Word"},"suffix":[{"text":"git","type":"Word"},{"text":"clone","type":"Word"},{"text":"--branch","type":"Word"},{"text":"\"\\$${dep}_BRANCH\"","expansion":[{"loc":{"start":3,"end":8},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"},{"text":"\"\\$${dep}_LOCATION\"","expansion":[{"loc":{"start":3,"end":8},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"},{"text":"\"\\$${dep}\"","expansion":[{"loc":{"start":3,"end":8},"parameter":"dep","type":"ParameterExpansion"}],"type":"Word"}]}]}}]}}
SET IFS=%OLDIFS%
echo "Running charsetguesser.c"
clang -I%YAML%/include -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -o charsetguesser.exe charsetguesser.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c 

echo "Running convtest.c"
clang ""
-I../h ""
-I../platform/windows ""
-D_CRT_SECURE_NO_WARNINGS ""
-Dstrdup=_strdup ""
-DYAML_DECLARE_STATIC=1 ""
-Wdeprecated-declarations ""
--rtlib=compiler-rt ""
-o "convtest.exe" ""
convtest.c ""
../c/charsets.c ""
../platform/windows/winfile.c ""
../c/timeutls.c ""
../c/utils.c ""
../c/alloc.c
echo "Running jqtest.c"
clang ""
-I../platform/windows ""
-I "%CD%.h" "-Wdeprecated-declarations" ""
-D_CRT_SECURE_NO_WARNINGS ""
-o "jqtest.exe" "jqtest.c" ""
../c/microjq.c ""
../c/parsetools.c ""
../c/json.c ""
../c/xlate.c ""
../c/charsets.c ""
../c/winskt.c ""
../c/logging.c ""
../c/collections.c ""
../c/timeutls.c ""
../c/utils.c ""
../c/alloc.c
ygsiudgus
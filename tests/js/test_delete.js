import { ConfigManager } from "Configuration";

/*
  Test the copyConfigurationAndDeleteKey functionality isolated.

  Usage (from zowe-common-c/):
    ./bin/configmgr -script tests/js/test_delete.js \
      -s "tests/schemadata/zoweappserver.json:tests/schemadata/zowebase.json:tests/schemadata/zowecommon.json" \
      -p "FILE(tests/schemadata/bundle1.json)"
*/

var getArg = function(key){
    for (let i=0; i<scriptArgs.length; i++){
        if ((scriptArgs[i] == key) && (i+1 < scriptArgs.length)){
            return scriptArgs[i+1];
        }
    }
    return null;
}

let cmgr = new ConfigManager();
cmgr.setTraceLevel(0);
const configName = "theConfig";
cmgr.addConfig(configName);
cmgr.setConfigPath(configName, getArg("-p"));
cmgr.loadSchemas(configName, getArg("-s"));
cmgr.loadConfiguration(configName);

let passed = 0;
let failed = 0;

function tryDelete(delCase) {
    const delCfgName = "delConfig_" + delCase + "_" + passed;
    let status = cmgr.copyConfigurationAndDeleteKey(configName, delCfgName, delCase);
    let [yamlStatus, text] = cmgr.writeYAML(delCfgName);
    if (status === 0 && yamlStatus === 0) {
        console.log("PASS: delete [" + JSON.stringify(delCase) + "]");
        passed++;
    } else {
        console.log("FAIL: delete [" + JSON.stringify(delCase) + "] status=" + status + " yamlStatus=" + yamlStatus);
        failed++;
    }
}

tryDelete("");
tryDelete("      ");
tryDelete("A");
tryDelete("colors");
tryDelete("D.A");
tryDelete("D");
tryDelete("E.A");
tryDelete("F.A");
tryDelete("F.C");
tryDelete("G[.A.B.C.D");
tryDelete("G[");
tryDelete("colors[0]");
tryDelete("colors.1a");
tryDelete("colors[5]");
tryDelete("colors[5].1");
tryDelete("G[.A.B.D[1]");
tryDelete("G[.A.B.D[1].C");
tryDelete("[_zsf.debugging.level]");
tryDelete("_zsf.debugging.level");
tryDelete("[_test.array[0]]");
tryDelete("[_test.array[2]]");
tryDelete("[_test.array[3]].[_test.nested[0]]");
tryDelete("key256aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
tryDelete("key which is defined");
tryDelete("key which is not defined");
tryDelete("ugly\nkey");
tryDelete(" ");
tryDelete("  ");

console.log("\nResults: " + passed + " passed, " + failed + " failed");

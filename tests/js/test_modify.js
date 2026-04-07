import { ConfigManager } from "Configuration";

/*
  Test: makeModifiedConfiguration overlays JSON onto existing config.

  Usage (from zowe-common-c/):
    ./bin/configmgr -script tests/js/test_modify.js \
      -s "tests/schemadata/zoweyaml.schema:tests/schemadata/zowecommon.json" \
      -p "FILE(tests/schemadata/example-zowe.yaml)"
*/

var getArg = function(key) {
    for (let i = 0; i < scriptArgs.length; i++) {
        if (scriptArgs[i] === key && i + 1 < scriptArgs.length) {
            return scriptArgs[i + 1];
        }
    }
    return null;
};

let passed = 0;
let failed = 0;

function assert(condition, msg) {
    if (condition) {
        console.log("PASS: " + msg);
        passed++;
    } else {
        console.log("FAIL: " + msg);
        failed++;
    }
}

let cmgr = new ConfigManager();
cmgr.setTraceLevel(0);
const configName = "base";
cmgr.addConfig(configName);
cmgr.setConfigPath(configName, getArg("-p"));
cmgr.loadSchemas(configName, getArg("-s"));
let status = cmgr.loadConfiguration(configName);
assert(status === 0, "Base config loaded");

let baseData = cmgr.getConfigData(configName);
const originalPort = baseData.zowe.externalPort;

// ---- makeModifiedConfiguration: change externalPort ----
const modName = "modified";
const newPort = 19999;
status = cmgr.makeModifiedConfiguration(
    configName, modName,
    { zowe: { externalPort: newPort } },
    1 /* array merge policy */
);
assert(status === 0, "makeModifiedConfiguration returns 0");

let modData = cmgr.getConfigData(modName);
assert(modData !== null, "modified config data is not null");
assert(modData.zowe.externalPort === newPort,
    "externalPort overridden to " + newPort + " (was " + originalPort + ")");

// ---- Original config is unchanged ----
let baseData2 = cmgr.getConfigData(configName);
assert(baseData2.zowe.externalPort === originalPort,
    "Original externalPort unchanged: " + originalPort);

// ---- writeYAML on the modified config ----
let [ys, yamlText] = cmgr.writeYAML(modName);
assert(ys === 0, "writeYAML on modified config returns 0");
assert(yamlText.indexOf("19999") >= 0, "YAML output contains new port 19999");

// ---- makeModifiedConfiguration: add a new key ----
const mod2Name = "modified2";
status = cmgr.makeModifiedConfiguration(configName, mod2Name, { newTestKey: "hello" }, 1);
assert(status === 0, "makeModifiedConfiguration with new key returns 0");
let mod2Data = cmgr.getConfigData(mod2Name);
assert(mod2Data.newTestKey === "hello", "New key 'newTestKey' has value 'hello'");

// ---- Summary ----
console.log("\nResults: " + passed + " passed, " + failed + " failed");
if (failed > 0) {
    throw new Error(failed + " test(s) failed");
}

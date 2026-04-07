import { ConfigManager } from "Configuration";

/*
  Test: schema validation with zoweyaml.schema + zowecommon.json

  Usage (from zowe-common-c/):
    ./bin/configmgr -script tests/js/test_validate.js \
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

// ---- Test 1: basic construction ----
let cmgr = new ConfigManager();
assert(cmgr !== null, "ConfigManager constructed");
cmgr.setTraceLevel(0);
assert(cmgr.getTraceLevel() === 0, "setTraceLevel/getTraceLevel roundtrip");

// ---- Test 2: addConfig ----
const configName = "zoweConfig";
let status = cmgr.addConfig(configName);
assert(status === 0, "addConfig returns 0");

// ---- Test 3: setConfigPath ----
const configPath = getArg("-p");
assert(configPath !== null, "Got -p arg: " + configPath);
status = cmgr.setConfigPath(configName, configPath);
assert(status === 0, "setConfigPath returns 0");

// ---- Test 4: loadSchemas ----
const schemaList = getArg("-s");
assert(schemaList !== null, "Got -s arg");
status = cmgr.loadSchemas(configName, schemaList);
assert(status === 0, "loadSchemas returns 0");

// ---- Test 5: loadConfiguration ----
status = cmgr.loadConfiguration(configName);
assert(status === 0, "loadConfiguration returns 0");

// ---- Test 6: validate ----
let validation = cmgr.validate(configName);
assert(typeof validation === "object", "validate returns object");
assert(validation.ok === true, "validation.ok is true");

// ---- Test 7: getConfigData ----
let configData = cmgr.getConfigData(configName);
assert(configData !== null, "getConfigData returns non-null");
assert(typeof configData === "object", "getConfigData returns object");
assert(configData.zowe !== undefined, "config has 'zowe' top-level key");
assert(typeof configData.zowe.logDirectory === "string", "zowe.logDirectory is a string");
assert(configData.zowe.logDirectory.length > 0, "zowe.logDirectory is non-empty: " + configData.zowe.logDirectory);

// ---- Test 8: writeYAML ----
let [yamlStatus, yamlText] = cmgr.writeYAML(configName);
assert(yamlStatus === 0, "writeYAML returns status 0");
assert(typeof yamlText === "string", "writeYAML returns string");
assert(yamlText.indexOf("zowe") >= 0, "YAML output contains 'zowe'");

// ---- Summary ----
console.log("\nResults: " + passed + " passed, " + failed + " failed");
if (failed > 0) {
    throw new Error(failed + " test(s) failed");
}

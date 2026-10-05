// config_path_reset.js -- cfgSetConfigPath() must SET the path, not append
// to it (zowe/zowe-common-c#571).
//
//   scriptArgs[3] = absolute path to permissive_schema.json
//   scriptArgs[4] = absolute path to path_reset_a.yaml   (a: 1, list: [one])
//   scriptArgs[5] = absolute path to path_reset_b.yaml   (b: 2, list: [two])
//
// The path is set to A, then to B. Only B may be loaded: before the fix the
// two lists were chained and both files merged, so `a` appeared and the list
// held both members.
import { ConfigManager } from "Configuration";
import * as std from "std";

const schema = scriptArgs[3];
const yamlA = scriptArgs[4];
const yamlB = scriptArgs[5];

let failures = 0;
function check(label, ok) {
  console.log((ok ? "PASS: " : "FAIL: ") + label);
  if (!ok) failures++;
}

let mgr = new ConfigManager();
mgr.setTraceLevel(0);
check("addConfig rc==0", mgr.addConfig("cfg") === 0);
check("first setConfigPath (A) rc==0", mgr.setConfigPath("cfg", "FILE(" + yamlA + ")") === 0);
check("second setConfigPath (B) rc==0", mgr.setConfigPath("cfg", "FILE(" + yamlB + ")") === 0);
check("loadSchemas rc==0", mgr.loadSchemas("cfg", schema) === 0);
check("loadConfiguration rc==0", mgr.loadConfiguration("cfg") === 0);

let data = mgr.getConfigData("cfg");
check("getConfigData returns an object", typeof data === "object" && data !== null);
check("key from the replaced path (a) is absent", data.a === undefined);
check("key from the current path (b) is present", data.b === 2);
check("list has exactly the current path's member", Array.isArray(data.list) && data.list.length === 1 && data.list[0] === "two");

console.log(failures === 0 ? "config_path_reset: all checks passed" : ("config_path_reset: " + failures + " check(s) FAILED"));
std.exit(failures === 0 ? 0 : 1);

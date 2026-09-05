// write_yaml_width.js -- writeYAMLWithWidth() keeps every output line within
// the given width by folding long double-quoted values with escaped line
// breaks, and the folded text reads back identical (zowe/zowe-common-c#550).
//
//   scriptArgs[3] = schema, [4] = long_values.yaml, [5] = width,
//   scriptArgs[6] = scratch path for the folded output,
//   scriptArgs[7] = long_key_unfoldable.yaml
import { ConfigManager } from "Configuration";
import * as std from "std";
import * as xplatform from "xplatform";

const schema = scriptArgs[3];
const yaml = scriptArgs[4];
const width = Number.parseInt(scriptArgs[5], 10);
const outPath = scriptArgs[6];
const unfoldable = scriptArgs[7];

let failures = 0;
function check(label, ok) {
  console.log((ok ? "PASS: " : "FAIL: ") + label);
  if (!ok) failures++;
}
function load(name, file) {
  let mgr = new ConfigManager();
  mgr.setTraceLevel(0);
  mgr.addConfig(name);
  mgr.setConfigPath(name, "FILE(" + file + ")");
  mgr.loadSchemas(name, schema);
  let rc = mgr.loadConfiguration(name);
  return [mgr, rc];
}
function longestLine(text) {
  return text.split("\n").reduce((m, l) => Math.max(m, l.length), 0);
}

let [mgr, rc] = load("cfg", yaml);
check("fixture loads", rc === 0);

let [plainStatus, plainText] = mgr.writeYAML("cfg");
check("writeYAML still works", plainStatus === 0 && typeof plainText === "string");
check("fixture has lines wider than " + width + " when unfolded", longestLine(plainText) > width);

let [status, text] = mgr.writeYAMLWithWidth("cfg", width);
check("writeYAMLWithWidth rc==0", status === 0 && typeof text === "string");
check("no line wider than " + width, longestLine(text) <= width);
check("output uses escaped line breaks", text.includes("\\\n"));

/* xplatform writes the file the way zwe does, tagged and converted on z/OS */
check("folded output stored", xplatform.storeFileUTF8(outPath, xplatform.AUTO_DETECT, text) === 0);
let [mgr2, rc2] = load("folded", outPath);
check("folded output loads", rc2 === 0);
check("folded output reads back identical",
      JSON.stringify(mgr2.getConfigData("folded")) === JSON.stringify(mgr.getConfigData("cfg")));

let [mgr3, rc3] = load("unfoldable", unfoldable);
check("unfoldable fixture loads", rc3 === 0);
let [status3, text3] = mgr3.writeYAMLWithWidth("unfoldable", width);
check("a key wider than the width is an error, not a truncation", status3 !== 0 && text3 === null);

console.log(failures === 0 ? "write_yaml_width: all checks passed" : ("write_yaml_width: " + failures + " check(s) FAILED"));
std.exit(failures === 0 ? 0 : 1);

// write_yaml_width.js -- writeYAMLWithWidth() keeps every output line within
// the given width by folding long double-quoted values with escaped line
// breaks, and the folded text reads back identical (zowe/zowe-common-c#550).
//
//   scriptArgs[3] = schema, [4] = long_values.yaml, [5] = width,
//   scriptArgs[6] = scratch path for the folded output,
//   scriptArgs[7] = long_key_unfoldable.yaml,
//   scriptArgs[8] = write_yaml_shapes.yaml, [9] = second scratch path
import { ConfigManager } from "Configuration";
import * as std from "std";
import * as xplatform from "xplatform";

const schema = scriptArgs[3];
const yaml = scriptArgs[4];
const width = Number.parseInt(scriptArgs[5], 10);
const outPath = scriptArgs[6];
const unfoldable = scriptArgs[7];
const shapes = scriptArgs[8];
const shapesOut = scriptArgs[9];

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

/* ---- shapes raised in review: templates, comments, flow arrays ---- */
let [mgr4, rc4] = load("shapes", shapes);
check("shapes fixture loads", rc4 === 0);
let data4 = mgr4.getConfigData("shapes");

/* A template is evaluated before the writer sees it, so a short line of YAML
   source can become an output line of any length: 24 bytes of source for
   `expanded` here, 180 bytes of value. An over-long output line therefore
   cannot be predicted by looking at the input file. */
check("a template evaluates to more than the width before writing",
      data4.zowe.expanded.length > width);
let [status4, text4] = mgr4.writeYAML("shapes");
check("unfolded shapes output exceeds the width", status4 === 0 && longestLine(text4) > width);

let [status5, text5] = mgr4.writeYAMLWithWidth("shapes", width);
check("shapes fold within the width", status5 === 0 && longestLine(text5) <= width);
check("shapes output stored",
      xplatform.storeFileUTF8(shapesOut, xplatform.AUTO_DETECT, text5) === 0);
let [mgr5, rc5] = load("shapesBack", shapesOut);
check("folded shapes load", rc5 === 0);
check("evaluated template values survive folding",
      JSON.stringify(mgr5.getConfigData("shapesBack")) === JSON.stringify(data4));

/* Comments are not part of the JSON model this writer is handed, so a long
   comment cannot reach the folder. Pinned deliberately: #582 added a second,
   comment-preserving writer that takes no width, so if the write path ever
   moves to it, this is the assertion that notices. */
check("comments are not emitted by this writer", text5.indexOf("parmlib record") < 0);

/* libyaml re-emits every non-empty sequence in block style, so a flow array
   in the input cannot produce a long line on output either. */
check("a flow array comes back in block style",
      text5.indexOf("[") < 0 && text5.indexOf("- 11") >= 0);

console.log(failures === 0 ? "write_yaml_width: all checks passed" : ("write_yaml_width: " + failures + " check(s) FAILED"));
std.exit(failures === 0 ? 0 : 1);

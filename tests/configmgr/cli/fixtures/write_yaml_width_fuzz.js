// write_yaml_width_fuzz.js -- randomized round trip for writeYAMLWithWidth()
// (zowe/zowe-common-c#550). Random values full of quotes, backslashes,
// spaces, tabs and YAML punctuation are written as YAML, loaded, folded at a
// random width, loaded again, and compared. Deterministic seed, so a failure
// reproduces with the same iteration number.
//
//   scriptArgs[3] = schema, [4] = iterations, [5] = scratch directory
import { ConfigManager } from "Configuration";
import * as std from "std";
import * as os from "os";
import * as xplatform from "xplatform";

const schema = scriptArgs[3];
const iterations = Number.parseInt(scriptArgs[4], 10);
const scratch = scriptArgs[5];

let seed = 20260905;
function rnd(n) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; }

const alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" +
                 "    \"\"\\\\''::##--//.,[]{}&*!|>%@`?~^$()=+;<>\t";
function randomValue() {
  const shape = rnd(4);
  let len = rnd(shape === 0 ? 8 : 260);
  let s = "";
  for (let i = 0; i < len; i++) {
    s += shape === 1 ? "/abcdefghij"[rnd(11)] : alphabet[rnd(alphabet.length)];
  }
  return s;
}
function randomKey(i) {
  let k = "k" + i + "_";
  let len = rnd(3) === 0 ? rnd(60) : rnd(10);
  for (let j = 0; j < len; j++) k += "abcdefghijklmnopqrstuvwxyz_"[rnd(27)];
  return k;
}
function load(name, file) {
  let mgr = new ConfigManager();
  mgr.setTraceLevel(0);
  mgr.addConfig(name);
  mgr.setConfigPath(name, "FILE(" + file + ")");
  mgr.loadSchemas(name, schema);
  return [mgr, mgr.loadConfiguration(name)];
}
function longestLine(text) {
  return text.split("\n").reduce((m, l) => Math.max(m, l.length), 0);
}

let failures = 0;
let folded = 0;
let refused = 0;
for (let it = 0; it < iterations; it++) {
  const obj = {};
  const count = 1 + rnd(6);
  for (let i = 0; i < count; i++) obj[randomKey(i)] = randomValue();
  const list = [];
  for (let i = 0; i < rnd(3); i++) list.push(randomValue());
  obj.list = list;
  obj.number = rnd(100000);

  let yaml = "zowe:\n";
  for (const k of Object.keys(obj)) {
    if (k === "list") {
      yaml += "  list:\n";
      for (const v of list) yaml += "    - " + JSON.stringify(v) + "\n";
    } else {
      yaml += "  " + JSON.stringify(k) + ": " + JSON.stringify(obj[k]) + "\n";
    }
  }
  const src = scratch + "/fuzz-src-" + it + ".yaml";
  const out = scratch + "/fuzz-out-" + it + ".yaml";
  xplatform.storeFileUTF8(src, xplatform.AUTO_DETECT, yaml);
  let [mgr, rc] = load("src", src);
  if (rc !== 0) { console.log("FAIL: iteration " + it + ": source did not load"); failures++; continue; }

  const width = 24 + rnd(80);
  let [status, text] = mgr.writeYAMLWithWidth("src", width);
  if (status !== 0) {
    /* legal only when a key alone does not fit */
    let widest = 0;
    for (const k of Object.keys(obj)) widest = Math.max(widest, k.length + 6);
    if (widest <= width) { console.log("FAIL: iteration " + it + ": refused width " + width + " although every key fits"); failures++; }
    else refused++;
    os.remove(src);
    continue;
  }
  if (longestLine(text) > width) { console.log("FAIL: iteration " + it + ": line wider than " + width); failures++; }
  xplatform.storeFileUTF8(out, xplatform.AUTO_DETECT, text);
  let [mgr2, rc2] = load("out", out);
  if (rc2 !== 0) { console.log("FAIL: iteration " + it + ": folded text did not load (width " + width + ")"); failures++; }
  else if (JSON.stringify(mgr2.getConfigData("out")) !== JSON.stringify(mgr.getConfigData("src"))) {
    console.log("FAIL: iteration " + it + ": data changed after folding at width " + width); failures++;
  } else folded++;
  os.remove(src); os.remove(out);
}

console.log("iterations=" + iterations + " folded=" + folded + " refused=" + refused + " failures=" + failures);
console.log(failures === 0 ? "write_yaml_width_fuzz: all iterations passed" : "write_yaml_width_fuzz: FAILED");
std.exit(failures === 0 ? 0 : 1);

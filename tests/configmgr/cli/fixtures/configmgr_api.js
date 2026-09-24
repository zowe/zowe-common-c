/*
 * test_configmgr_api.sh's payload script. Exercises the embedded JS
 * Configuration native module and asserts on contract details: return
 * shapes, return codes, side-effect visibility across method calls,
 * idempotency, and behavior for unknown/missing inputs.
 *
 * Each assertion prints a single "PASS:" or "FAIL:" line. Exit via
 * throw at the end if anything failed.
 *
 * Fixtures referenced (passed via scriptArgs):
 *   scriptArgs[3] = absolute path to port_range_schema.json
 *   scriptArgs[4] = absolute path to port_good.yaml
 *   scriptArgs[5] = absolute path to port_too_large.yaml
 *   scriptArgs[6] = absolute path to permissive_schema.json
 *   scriptArgs[7] = absolute path to types.yaml
 */
import { ConfigManager } from "Configuration";

const PORT_SCHEMA = scriptArgs[3];
const PORT_GOOD   = scriptArgs[4];
const PORT_BAD    = scriptArgs[5];
const ANY_SCHEMA  = scriptArgs[6];
const TYPES_YAML  = scriptArgs[7];

let pass = 0;
let fail = 0;
function check(label, ok, detail) {
  if (ok) {
    console.log("PASS: " + label);
    pass++;
  } else {
    console.log("FAIL: " + label + (detail ? "  " + detail : ""));
    fail++;
  }
}

// ----- 1: lifecycle: instantiate, isolate instances -----
let mgrA = new ConfigManager();
let mgrB = new ConfigManager();
check("1.1 two instances are distinct", mgrA !== mgrB);

// trace level is per-instance
mgrA.setTraceLevel(2);
mgrB.setTraceLevel(0);
check("1.2 setTraceLevel/getTraceLevel A == 2", mgrA.getTraceLevel() === 2);
check("1.3 setTraceLevel/getTraceLevel B == 0", mgrB.getTraceLevel() === 0);
mgrA.setTraceLevel(0);

// ----- 2: addConfig contract -----
let rc = mgrA.addConfig("good");
check("2.1 addConfig 'good' rc==0", rc === 0);

rc = mgrA.addConfig("good"); // duplicate
check("2.2 addConfig duplicate name rc==0 (idempotent)", rc === 0);

// ----- 3: setConfigPath / loadSchemas / loadConfiguration positive -----
rc = mgrA.setConfigPath("good", "FILE(" + PORT_GOOD + ")");
check("3.1 setConfigPath rc==0", rc === 0);

rc = mgrA.loadSchemas("good", PORT_SCHEMA);
check("3.2 loadSchemas rc==0", rc === 0);

rc = mgrA.loadConfiguration("good");
check("3.3 loadConfiguration rc==0", rc === 0);

// ----- 4: validate (good config) -----
// The validate() return shape is a DELIBERATE two-signal contract:
//   ok: true,  exceptionTree: undefined  -> config is valid
//   ok: true,  exceptionTree: present    -> config has schema violations
//   ok: false                            -> validator itself had an internal failure
// So ok=true does NOT mean "config is valid"; it means "validator ran cleanly."
// zowe-install-packaging's TypeScript layer (bin/libs/configmgr.ts and
// bin/libs/component.ts) consume both signals together. Don't "fix" this.
let v = mgrA.validate("good");
check("4.1 validate returns an object", typeof v === "object");
check("4.2 validate(good).ok === true (validator ran cleanly)", v.ok === true);
check("4.3 validate(good) has no exceptionTree", v.exceptionTree === undefined);
// shoeSize was a debugging artifact removed 2026-05-13; assert it's gone.
check("4.4 validate response has no shoeSize debug field",
      v.shoeSize === undefined);

// ----- 5: getConfigData returns the post-template-eval JSON -----
let data = mgrA.getConfigData("good");
check("5.1 getConfigData returns an object", typeof data === "object");
check("5.2 data.port === 8080", data.port === 8080);

// ----- 6: validate on a BAD config exposes exceptionTree -----
rc = mgrA.addConfig("bad");
mgrA.setConfigPath("bad", "FILE(" + PORT_BAD + ")");
mgrA.loadSchemas("bad", PORT_SCHEMA);
mgrA.loadConfiguration("bad");
let vBad = mgrA.validate("bad");
// Per the contract documented above 4.2: ok=true here means the validator
// ran cleanly. The "this config has problems" signal is exceptionTree.
check("6.1 validate(bad).ok === true (validator ran; problems carried by exceptionTree)",
      vBad.ok === true);
check("6.2 validate(bad).exceptionTree is present",
      vBad.exceptionTree !== undefined);
check("6.3 validate(bad).exceptionTree.message exists",
      vBad.exceptionTree && typeof vBad.exceptionTree.message === "string");
check("6.4 validate(bad).exceptionTree has subExceptions array",
      vBad.exceptionTree && Array.isArray(vBad.exceptionTree.subExceptions));

// Walk the exception tree looking for "too large" text
let foundTooLarge = false;
function walk(e) {
  if (!e) return;
  if (typeof e.message === "string" && e.message.indexOf("too large") >= 0) {
    foundTooLarge = true;
  }
  if (e.subExceptions) for (const c of e.subExceptions) walk(c);
}
walk(vBad.exceptionTree);
check("6.5 exception tree mentions 'too large' (the port=999999 violation)",
      foundTooLarge);

// ----- 7: unknown config name returns a sensible error -----
rc = mgrA.loadConfiguration("never-added");
check("7.1 loadConfiguration on unknown name returns non-zero", rc !== 0);
// ZCFG_UNKNOWN_CONFIG_NAME == 3
check("7.2 loadConfiguration returns ZCFG_UNKNOWN_CONFIG_NAME (3)", rc === 3);

// ----- 8: makeModifiedConfiguration produces an overlay -----
rc = mgrA.makeModifiedConfiguration("good", "good_mod",
                                    { port: 1234, extra: "added" }, 1);
check("8.1 makeModifiedConfiguration rc==0", rc === 0);
let modData = mgrA.getConfigData("good_mod");
check("8.2 mod's port overridden to 1234", modData.port === 1234);
check("8.3 mod has new key 'extra' == 'added'", modData.extra === "added");
// Original config unchanged
let origData = mgrA.getConfigData("good");
check("8.4 original 'good' config NOT mutated", origData.port === 8080);

// ----- 9: copyConfigurationAndDeleteKey -----
rc = mgrA.copyConfigurationAndDeleteKey("good", "good_minus", "port");
check("9.1 copyConfigurationAndDeleteKey rc==0", rc === 0);
let minusData = mgrA.getConfigData("good_minus");
check("9.2 deleted key 'port' absent from 'good_minus'",
      minusData.port === undefined);
// Original unchanged
check("9.3 original 'good' still has port",
      mgrA.getConfigData("good").port === 8080);

// ----- 10: writeYAML round-trip -----
let [yamlStatus, yamlText] = mgrA.writeYAML("good");
check("10.1 writeYAML status == 0", yamlStatus === 0);
check("10.2 writeYAML produced text", typeof yamlText === "string" && yamlText.length > 0);
check("10.3 yaml text mentions port: 8080",
      yamlText.indexOf("port") >= 0 && yamlText.indexOf("8080") >= 0);

// ----- 11: template eval is in effect at getConfigData time -----
rc = mgrA.addConfig("typed");
mgrA.setConfigPath("typed", "FILE(" + TYPES_YAML + ")");
mgrA.loadSchemas("typed", ANY_SCHEMA);
rc = mgrA.loadConfiguration("typed");
check("11.1 typed config loadConfiguration rc==0", rc === 0);
let typedData = mgrA.getConfigData("typed");
check("11.2 template_int evaluated to 117 by getConfigData",
      typedData.template_int === 117);
check("11.3 template_bool evaluated to true", typedData.template_bool === true);
check("11.4 template_object/foo == bar",
      typedData.template_object && typedData.template_object.foo === "bar");
check("11.5 template_array_simple is a [1,2,3] array",
      Array.isArray(typedData.template_array_simple) &&
      typedData.template_array_simple.length === 3 &&
      typedData.template_array_simple[0] === 1 &&
      typedData.template_array_simple[2] === 3);

// ----- summary -----
console.log("");
console.log("---- JS API CONTRACT: " + pass + " pass, " + fail + " fail ----");
if (fail > 0) {
  throw new Error("JS API contract: " + fail + " assertion(s) failed");
}

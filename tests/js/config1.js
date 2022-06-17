import * as os from 'os';
import { ConfigManager } from "Configuration";

console.log("hello ConfigMgr, args were ["+scriptArgs+"]");

/*
  Test command lines:

  From zowe-common-c/c

  configmgr -script ../tests/js/config1.js -s "../tests/schemadata/zoweappserver.json:../tests/schemadata/zowebase.json:../tests/schemadata/zowecommon.json" -p "FILE(../tests/schemadata/bundle1.json)" 

  Testing ZSS schemas

  ../deps/zowe-common-c/bin/configmgr -script ../deps/zowe-common-c/tests/js/config1.js -s "../schemas/zowe-schema.json:../schemas/zowe-yaml-schema.json:../schemas/server-common.json:../schemas/zss-config.json" -p "FILE(../deps/zowe-common-c/tests/schemadata/bigyaml.yaml)"

 */

var getArg = function(key){
    for (let i=0; i<scriptArgs.length; i++){
        if ((scriptArgs[i] == key) && (i+1 < scriptArgs.length)){
            return scriptArgs[i+1];
        }
    }
    return null;
}

var showExceptions = function(e,depth){
    let blanks = "                                                                 ";
    let subs = e.subExceptions;
    console.log(blanks.substring(0,depth*2)+e.message);
    if (subs){
        for (const sub of subs){
            showExceptions(sub,depth+1);
        }
    }
}

var loadAndExtract = function(){
    let cmgr = new ConfigManager();
    cmgr.setTraceLevel(0);
    let configName = "theConfig";
    let status = 0;
    let configPath = getArg("-p");
    let schemaList = getArg("-s");
    if (!configPath){
        console.log("specify a config path with '-p '");
    }
    if (!schemaList){
        console.log("specify a schema list with '-s '");
    }
    console.log("config path is "+configPath);
    status = cmgr.addConfig(configName);
    if (status != 0){
        console.log("could not add config");
        return;
    }
    status = cmgr.setConfigPath(configName,configPath);
    if (status != 0){
        console.log("could not set config path: "+configPath);
        return;
    }
    cmgr.setTraceLevel(0);
    status = cmgr.loadSchemas(configName,schemaList);
    if (status != 0){
        console.log("could not loadSchemas: "+schemaList);
        return;
    }
    console.log("ready to load config data");
    status = cmgr.loadConfiguration(configName);
    if (status != 0){
        console.log("could not loadConfiguration status="+status);
        return;
    }

    let validation = cmgr.validate(configName); // { ok: <bool> ( exceptions: <array> )? }
    console.log("validator ran to completion "+validation.ok);
    if (validation.ok){
        if (validation.exceptionTree){
            console.log("validation found invalid JSON Schema data");
            showExceptions(validation.exceptionTree,0);
        } else {
            console.log("no exceptions seen");
            let theConfig = cmgr.getConfigData(configName);
            console.log("configData is loaded \n"+JSON.stringify(theConfig,null,"\n"));
            console.log("listenerPort is "+theConfig.listenerPort);
            let [ yamlStatus, textOrNull ] = cmgr.writeYAML(configName);
            console.log("here's the whole config as yaml, status="+status);
            if (yamlStatus == 0){
              console.log(""+textOrNull);
            }
        }
    } else {
        console.log("validation failed, contact Zowe support");
    }

}

loadAndExtract();


import * as os from 'os';
import { ConfigManager } from "Configuration";

console.log("hello ConfigMgr, args were ["+scriptArgs+"]");

/*
  Test command lines:

  From zowe-common-c/c

  configmgr -script ..tests/js/config1.js -s "../tests/schemadata/zoweappserver.json:../tests/schemadata/zowebase.json:../tests/schemadata/zowecommon.json" -p "FILE(../tests/schemadata/bundle1.json)" 
 */

var getArg = function(key){
    for (let i=0; i<scriptArgs.length; i++){
        if ((scriptArgs[i] == key) && (i+1 < scriptArgs.length)){
            return scriptArgs[i+1];
        }
    }
    return null;
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
        if (validation.exceptions){
            console.log("validation found invalid JSON Schema data");
            for (let i=0; i<validation.exceptions.length; i++){
                console.log("    "+validation.exceptions[i]);
            }
        } else {
            console.log("no exceptions seen");
            let theConfig = cmgr.getConfigData(configName);
            console.log("configData is loaded \n"+theConfig);
            console.log("listenerPort is "+theConfig.listenerPort);
        }
    } else {
        console.log("validation failed, contact Zowe support");
    }

}

loadAndExtract();
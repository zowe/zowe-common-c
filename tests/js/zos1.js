import * as zos from 'zos';

var test1 = function(filename){
  let zstats = zos.zstat(filename);
  console.log("zosStat => "+JSON.stringify(zstats));

  console.log("turning on PROGCTL ");

  /* Turning on the PROGCTL Bit */
  zos.changeExtAttr(filename, zos.EXTATTR_PROGCTL, true);

  zstats = zos.zstat(filename);
  console.log("zosStat => "+JSON.stringify(zstats));
}

var dispatch = function(){
  console.log("Start ZOS Test "+scriptArgs[3]);
  if (scriptArgs[3] == "stat"){
    test1(scriptArgs[4]);
  } else if (scriptArgs[3] == "dslist"){
    let result = zos.dslist(scriptArgs[4]);
    console.log("dslist: "+JSON.stringify(result));
  }
}

dispatch();

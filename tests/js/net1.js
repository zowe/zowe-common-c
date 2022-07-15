import * as zos from 'zos';
import * as os from 'os';
import * as net from 'net';

/*
  example.com == 93.184.216.34 

  ../bin/configmgr -script ../tests/js/net1.js https  93.184.216.34 443 "/"
  ../bin/configmgr -script ../tests/js/net1.js http  93.184.216.34 80 "/"

  ../bin/configmgr -script ../tests/js/net1.js tcp  93.184.216.34 80 

  ../bin/configmgr -script ../tests/js/net1.js tcp  93.184.216.34 800

  */

var testHTTP = function(args, index){
  let scheme = args[index+0];
  let host = args[index+1];
  let port = parseInt(args[index+2]);
  let path = args[index+3];
  let [ result, status ] = net.httpContentAsText(scheme,host,port,path);
  if (status == 0){
    console.log("content\n"+result);
  } else{
    console.log("http client failed with status = "+status);
  }
}

var testTCP = function(args, index){
  let host = args[index+1];
  let port = parseInt(args[index+2]);
  let waitMS = parseInt(args[index+3]);
  let status = net.tcpPing(host,port,waitMS);
  console.log("tcp connect status = "+JSON.stringify(status));
  
}

var dispatch = function(args, index){
  console.log("Start Net Test "+scriptArgs[3]);
  if (scriptArgs[3] == "http" ||
      scriptArgs[3] == "https" ){
    let configStatus = net.tlsClientConfig({ truststore: "../tests/javatrust.p12", password: "password"});
    console.log("tls config status = "+configStatus);
    testHTTP(scriptArgs,3); 
  } else if (scriptArgs[3] == "tcp"){
    testTCP(scriptArgs,3);
  }
}

dispatch();

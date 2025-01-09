#!/usr/bin/env/bash
set -e
zowex job list
echo " "
jobid=$(zowex job submit "dkelosky.jcl(iefbr14)" --only-jobid true)
echo "Submitted job ${jobid}"
echo " "
sleep 1
zowex job list-files ${jobid}
echo " "
zowex job view-file ${jobid} 2
echo " "
zowexx console issue "d iplinfo" --console-name zowe
echo " "

import * as zos from 'zos';

console.log(`Resolved static symbol &SYSNAME to "${zos.resolveSymbol('&SYSNAME')}"`);
console.log(`Resolved dynamic symbol &DAY to "${zos.resolveSymbol('&DAY')}"`);
console.log('done');
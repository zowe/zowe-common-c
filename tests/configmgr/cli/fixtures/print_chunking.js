// print_chunking.js -- regression coverage for js_native_print() buffer
// chunking (c/embeddedjs.c, PRINT_STACK_BUF_SIZE=256 / MAX_PRINT_ELT_SIZE=0x10000).
//
// console.log() arguments up to 256 bytes are copied into a stack buffer;
// anything larger uses a 64KB heap buffer, chunked in 64KB pieces if the
// argument itself is bigger than that. This script prints one line per
// size class at the boundaries where that logic can go wrong, each
// bracketed by "<<<label:len>>>" / "<<<end>>>" markers so the wrapping
// shell test (test_print_chunking.sh) can slice out exactly the payload
// and check its length and content.
//
// Payloads are ('A' * (len-1)) + 'Z': a single wrong/dropped/duplicated
// byte anywhere shows up either as a length mismatch, a non-'A' byte in
// the body, a missing trailing 'Z', or (if bytes leaked across a chunk
// boundary into the next line) a shifted/missing "<<<end>>>" marker.

function makePayload(len) {
  return len <= 1 ? "Z".slice(0, len) : "A".repeat(len - 1) + "Z";
}

function emit(label, len) {
  console.log("<<<" + label + ":" + len + ">>>");
  console.log(makePayload(len));
  console.log("<<<end>>>");
}

// small text -- well under the 256-byte stack buffer
emit("small", 11);

// exactly the stack-buffer boundary
emit("stack-boundary", 256);

// one byte over the stack buffer -- switches to the heap buffer
emit("heap-boundary-plus1", 257);

// exactly one full 64KB chunk
emit("chunk-exact", 65536);

// one byte over a full chunk -- two chunks (65536 + 1)
emit("chunk-plus1", 65537);

// several chunks with an uneven remainder
emit("multi-chunk", 150000);

// text combined with numbers, a boolean, null and undefined -- several
// parameters in one call
console.log("<<<mixed-args>>>");
console.log("count:", 42, "pi:", 3.14159, "flag:", true, "nothing:", null, "missing:", undefined);
console.log("<<<end>>>");

// several plain parameters in one call
console.log("<<<several-params>>>");
console.log(1, 2, 3, "four", 5, "six");
console.log("<<<end>>>");

console.log("<<<done>>>");

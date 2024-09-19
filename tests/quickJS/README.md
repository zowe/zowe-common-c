# QuickJS tests

## Simple unit tests

This framework is as unit testing for individual javascript functions.

## Adding new test

Follow the existing structure, always returning the number of errors and tests.

Keep in mind, test the simple cases like:
* Working cases
* No parameters when exepecting some
* Incorrect data types
* Empty, null, undefined values...

```javascript
export function test_someFunction() {
    const TEST = [ -1, 0, 1, 42 ]
    let errs = 0;
    for (let t in TEST) {
        const result = someFunction(TEST[t]);
        print.clog(result == TEST[t], `someFunction(${TEST[i]})=${result}`);
        if (result != TEST[t]) {
            errs++;
        }
    }
    return { errors: errs, total: TEST.lenght }
}

```
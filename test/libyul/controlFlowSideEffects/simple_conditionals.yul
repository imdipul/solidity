{
    function a() {
        if calldataload(0) { g() }
    }
    function b() {
        g()
        if calldataload(0) { }
    }
    function c() {
        if calldataload(0) { }
        g()
    }
    function d() {
        stop()
        if calldataload(0) { g() }
    }
    function e() {
        if calldataload(0) { g() }
        stop()
    }
    function f() {
        g()
        if calldataload(0) { g() }
    }
    function g() { revert(0, 0) }
}
// ----
// a:never terminates, never reverts, never loops
// b:never terminates, never reverts, never loops
// c:never terminates, never reverts, never loops
// d:never terminates, never reverts, never loops
// e:never terminates, never reverts, never loops
// f:never terminates, never reverts, never loops
// g:never terminates, never reverts, never loops

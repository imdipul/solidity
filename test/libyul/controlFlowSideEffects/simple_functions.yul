{
    function a() {}
    function f() { g() }
    function g() { revert(0, 0) }
    function h() { stop() }
    function i() { h() }
    function j() { h() g() }
    function k() { g() h() }
}
// ----
// a:never terminates, never reverts, never loops
// f:never terminates, never reverts, never loops
// g:never terminates, never reverts, never loops
// h:never terminates, never reverts, never loops
// i:never terminates, never reverts, never loops
// j:never terminates, never reverts, never loops
// k:never terminates, never reverts, never loops

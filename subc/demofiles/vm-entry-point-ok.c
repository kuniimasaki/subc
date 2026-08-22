// vm-entry-point-ok: the -O flag (bytecode VM) previously never actually
// executed the user's main() at all -- apply()/eval() (the tree-walker)
// ran unconditionally regardless of -O, and separately, -O's only real
// effect (compiling standalone top-level REPL forms) crashed immediately
// on the most basic `return 42;` (compileOn had no case for Return) even
// if it had been wired up. Confirmed this predates any change this
// session (main_orig crashes identically under -O).
//
// Fixed: compileOn's Return case now emits the value followed by iRETURN;
// compileOn's Function case now runs typeCheck() on itself (closing the
// "-O isn't even type-checked" gap for whatever it compiles) so the
// function's name/parameters/type get resolved and it gets declared into
// scope, and binds the resulting closure onto the function name's
// Symbol,value (which is what a later call by that name -- iGETGVAR --
// actually reads); the interpreter's C main() now runs the user's main()
// through the exact iCALL/iRETURN machinery any nested call already uses
// (iPUSH a closure over the entry function, iCALL it, iHALT) instead of
// apply(), when -O is set.

int add(int a, int b) {
  return a + b;
}

int main() {
  return add(19, 23);
}

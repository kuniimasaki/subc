# subc bytecode VM: audit and memory-safety design

Scope: research/design only. No changes were made to `main.leg` or `main.c`.

> **Update (2026-08-21, after this audit was written):** the tree-walker's
> memory-safety checks have since been unified behind a `requireNotFreed(oop base,
> char *action)` helper (commit `4d4638f`), used by `getPointer`, `setMemory`, the
> `*p = rhs` assignment path, and `printiln`. The design comparison and
> recommendation in section 3 below (funnel VM pointer ops through the same
> primitives / helper) are unaffected — if anything, `requireNotFreed` existing now
> makes Option B's "single source of truth" goal concretely achievable.
Source examined: `main.leg` (single-file grammar + evaluator, ~4900 lines),
VM section roughly lines 4301-4776, cross-referenced against the tree-walker
(`eval`/`assign`/`apply`/`getPointer`/`setMemory`/`prim_free`, lines ~2180-3830
and the `-O` wiring in `replFile`/`main` at the end of the file).

## 1. What the VM currently is

`opt_O` (declared line 185, incremented by `-O` at line 4859) does **not**
change how the program's own `main()` is executed. The interpreter's C
`main()` (around line 4893-4932) unconditionally does:

```c
oop entry = Scope_lookup(intern("main"));
...
oop result = apply(entry, args, nil);   // always the tree walker
```

`apply()` (line 2180) always calls `eval(body)` for a `Function` — it never
looks at `Function,code` (the bytecode slot) and is completely independent of
`opt_O`. The only place `-O` has any effect is `replFile()` (line ~4786):
each top-level form read by the REPL/file loader is, if `-O` is set, handed
to `compile()` + `execute()` **instead of** `typeCheck()` + `preval()`:

```c
if (opt_O) {
    oop program = compile(yysval);
    result = execute(program);
}
else {
    ... typeCheck(yysval, nil); ... result = preval(yysval); ...
}
```

Two consequences that matter a lot for the gap assessment:

- **`typeCheck`/`preval` are skipped entirely in `-O` mode.** The VM compiles
  the *raw parser output*, not the resolved/declared tree the tree-walker
  works on. All the machinery that turns a bare `Symbol` into a bound
  `Variable`/`Constant`/`Function` (via `Scope_lookup`, `declareVariable`,
  etc.) never runs for `-O` input.
- **The VM's "globals" are a separate storage mechanism from the
  tree-walker's globals.** `iGETGVAR`/`iSETGVAR` (lines 4469-4488) first
  check a runtime alist (`assoc(env, operand)`, populated only by `iCALL`'s
  parameter binding), and otherwise read/write `Symbol,value` directly —
  i.e. a value stashed on the symbol object itself. The tree-walker instead
  resolves identifiers through `Scope_lookup` into `Variable` objects held in
  the `scopes` list (see `declare`/`declareVariable`, `apply()` line
  2207-2219). These two variable universes do not share state.
- Because `Function,code` (set by `compileOn`'s `case Function:`, line
  4749-4755) is only ever read inside `execute()`'s own `iCALL` handling of a
  `Closure` (line 4521), compiled function bodies are only reachable by code
  that is *itself* already running inside the VM (e.g. a REPL form that
  compiles and immediately calls a previously-defined function). A normal
  `subc -O program.c` run reaches the user's `main` via `apply()`/`eval()`
  regardless, so that stashed bytecode is dead weight in the intended "run a
  C program" use case today.

In short: the VM is a self-contained toy evaluator for simple expression/
statement forms exercised through the REPL loop, not an alternate back end
for the interpreter's real entry point yet.

## 2. Opcode inventory

Opcodes are defined at lines 4301-4310 (`enum opcode_t`), with matching
`disassemble()` cases (4319-4362) and `execute()` handlers (4365-4554).

| Opcode | Operand | Semantics |
|---|---|---|
| `iHALT` | – | Stop; expects exactly one value on the stack, returned as the program result. |
| `iPUSH` | literal `oop` | Push a compile-time constant/value. |
| `iPOP` | – | Discard top of stack. |
| `iNOT` | – | Logical not (`isFalse`). |
| `iCOM` | – | Bitwise complement (integers only). |
| `iNEG` | – | Arithmetic negate (int or float). |
| `iDEREF` | – | **`assert(!"unimplemented")`** — pointer dereference not implemented. |
| `iINDEX` | – | **`assert(!"unimplemented")`** — array/pointer indexing not implemented. |
| `iMUL`/`iDIV`/`iMOD` | – | Binary arithmetic, int or float (`iMOD` has a float `fmod` path). |
| `iADD`/`iSUB` | – | Binary arithmetic, int or float. |
| `iSHL`/`iSHR`/`iAND`/`iXOR`/`iOR` | – | Integer-only bitwise ops. |
| `iLT`/`iLE`/`iGE`/`iGT`/`iEQ`/`iNE` | – | Comparisons, int or float, push boolean. |
| `iGETGVAR` | `Symbol` | Look up value: check the call-frame alist (`env`) via `assoc`, else fall back to `Symbol,value`. Despite the name, this is really "get lexical-or-global-via-symbol-slot", not the tree-walker's global scope. |
| `iSETGVAR` | `Symbol` | Mirror of `iGETGVAR` for stores; same dual-lookup with the same caveat. |
| `iCLOSE` | `Function` node | Push `newClosure(func, env)` capturing the current alist environment. |
| `iCALL` | argc | Pop callee; if `Primitive`, call the C function directly with the raw stack slice (this is the *same* `prim_*` dispatch the tree-walker uses, so e.g. `malloc`/`free` calls that reach this opcode do run the real primitive); if `Closure`, bind params into a new alist environment and push a call frame (fixed-size array of 32 frames, no growth, `fatal()` on overflow). |
| `iRETURN` | – | Pop a call frame (restore `env`/`code`/`pc`). Note: there is **no compiler support** for emitting this from a C `return` statement — see below. |
| `iJMP` | target pc | Unconditional jump. |
| `iJMPF` | target pc | Pop condition, jump if falsy. |

Runtime state: a **fixed 32-slot value stack** and a **fixed 32-slot call
frame stack** (`stack[32]`, `frames[32]`, lines 4370-4380), both hard limits
with `fatal()`/`stackError()` on overflow — no dynamic growth.

## 3. `compileOn` coverage (what `compile()` can turn into bytecode)

`compileOn` (line 4565) switches over the full `type_t` enum (48 node kinds,
listed at line 63-74: `_do_types`). Anything not listed below hits
`assert(!"unimplemented")` (confirmed at least at lines 4573, 4574, 4575,
4579, 4580, 4582, 4608, 4609, 4610, 4617-4620, 4626-4627, 4649-4651, 4654,
4655, 4665, 4694, 4714, 4731-4747 — i.e. far more than the two markers
mentioned in the task brief; the two originally spotted (~4573, ~4654) are
just the first instances of a pattern that recurs about 30 times).

**Implemented / working:**
- Literals: `Undefined`, `Input`, `Integer`, `Float`, `String`, `Pair`,
  `Primitive`, `Closure` — all just `iPUSH` the literal `oop`.
- `Symbol` — `iGETGVAR` (see caveat above: not the tree-walker's scoping).
- `Call` — compiles argument pushes + callee + `iCALL`. Works generically
  for any callee that resolves to a `Primitive` or `Closure` at runtime,
  including calls to `malloc`/`free`/etc. *as bare calls*.
- `Block` — sequences statements, discarding all but the last value.
- `Unary` — only `NEG`/`NOT`/`COM`. `PREINC`/`PREDEC`/`POSTINC`/`POSTDEC` are
  unimplemented (no `++`/`--`).
- `Binary` — all arithmetic/relational/bitwise ops (`MUL`...`BOR`).
  `LAND`/`LOR` (`&&`/`||`) are explicitly unimplemented (no short-circuit
  codegen support).
- `Assign` — only the trivial case of `symbol = expr` (`iSETGVAR`); the
  compiler does not attempt to compile the lhs as an lvalue, so assignment
  through a pointer, index, or struct member cannot be compiled.
- `While` — full support including `break`/`continue` (via the `cs`/`bs`
  label lists threaded through `compileOn`).
- `If` — full support (both branches, with a `nil` else if absent handled
  upstream by the parser/AST, not specially here).
- `Continue`/`Break` — supported, but only meaningful inside a `While`
  compiled in the same call (see `For`, below).
- `Function` — compiles the body via `compileFunction` and emits `iCLOSE`;
  this is the mechanism that stashes bytecode in `Function,code`, subject to
  the reachability caveat in §1.

**Explicitly unimplemented (`assert(!"unimplemented")`):**
- `Pointer`, `Array`, `Struct`, `Memory`, `Reference` — i.e. **none of the
  runtime value representations the tree-walker uses for pointers, arrays,
  structs, or raw memory blocks can even be pushed as a literal**, let alone
  operated on.
- `List` (an internal-bookkeeping node, not user-facing).
- `Addressof`, `Dereference` — no `&x` / `*p`.
- `Sizeof` — no `sizeof`.
- `Index` — no `a[i]`.
- `Member` — no `s.f` / `p->f`.
- `Cast` — no `(int)x` etc.
- `For` — no `for` loops at all (only `while`).
- `Return` — **no `return` statements can be compiled inside a function
  body.** Combined with `Call`/`iCALL`/`iRETURN` existing at the VM level,
  this means the opcode plumbing for calls/returns is there, but the
  front end can't emit an early return — a function body can only produce
  its last-statement value.
- All 12 type nodes `Tvoid`...`Tetc`, plus `TypeName`, `VarDecls`,
  `TypeDecls` — **no declarations of any kind** (no `int x;`,
  `int *p;`, `struct S s;`, function prototypes, etc.) can be compiled.
  This is what actually blocks pointers/arrays/structs from ever showing
  up: even if `Pointer`/`Array`/`Struct` literal-pushing were implemented,
  there is no way to compile the declaration that would create the local
  in the first place.
- `Variable`, `Constant` — the *resolved* forms of an identifier that the
  tree-walker's `typeCheck`/`preval` pass produces (see the several
  `case Variable:` sites in `eval`/`assign`/`typeCheck`, e.g. lines 552,
  1363, 1687, 2241/2245, 2367, 3067, 3378, 3719, 3768, 4091, 4289).
  Since `-O` skips `typeCheck`/`preval` (§1), `compileOn` never actually
  needs to handle these in the current pipeline, but the switch case is
  present and asserts if it's ever hit — e.g. if a `Function` body being
  compiled were to reference its own parameters, they arrive as bare
  `Symbol`s at parse time and only become `Variable` post-resolution, so
  this is currently a latent trap rather than a live path.
- `Scope` — marked `assert(!"this cannot happen")`, i.e. not expected to
  reach codegen at all (internal bookkeeping only).

## 4. Gap assessment: how far is "VM feature-complete"?

**Large effort**, not small or medium, for three independent reasons that
each individually are substantial:

1. **Declarations and types are entirely absent from codegen.** Every single
   type node and both declaration node kinds (`VarDecls`/`TypeDecls`) assert.
   Without these, a compiled program cannot introduce a local variable,
   pointer, array, or struct — the front end can only shuffle values that
   already exist as literals or arrive as call arguments. This alone is
   comparable in size to writing a second, from-scratch lowering pass for
   declarations (including scope/lifetime handling, which the tree-walker
   currently does with `Scope_begin`/`Scope_end`/`declareVariable` — none of
   which the VM's frame model uses or has an equivalent for).
2. **The VM has no notion of the tree-walker's variable/scope model.**
   `iGETGVAR`/`iSETGVAR` use a parallel, incompatible storage scheme
   (call-frame alist plus ad hoc `Symbol,value`). Making the VM
   interoperate with real subc programs means either (a) migrating VM
   variable access to go through `Scope_lookup`/`Variable` objects the way
   the tree-walker does, or (b) keeping the alist scheme but proving it can
   correctly model C's block/function scoping, globals, and `&local`
   (address-of-local) semantics that `getPointer`'s `case Variable:` (line
   3209-3212) relies on. Either way this is a redesign, not a patch.
3. **The entry point never routes through the VM.** Fixing `apply()`
   (or the `-O` wiring) to actually execute user `main()` via `execute()`
   is a prerequisite before any of the above matters for real programs —
   currently `-O` only affects standalone top-level REPL forms and even
   then bypasses `typeCheck`/`preval`, which means today's `-O` runs are not
   even type-checked. Wiring the VM into `apply()` means deciding what
   happens to `Primitive` calls, varargs (`s_etc`), and closures created
   from tree-walker-declared functions (`Function,body` vs `Function,code`
   consistency).

On top of that, everything needed for the memory-safety research goal —
`Pointer`, `Array`, `Struct`, `Memory`, `Addressof`, `Dereference`,
`Index`, `Member`, `Cast`, `Sizeof` — is in the "explicitly unimplemented"
list. None of it is stubbed with partial logic; it is 100% missing. Getting
the VM to a point where it can run the kind of malloc/free/pointer-heavy
test programs this project cares about requires implementing essentially
all of §3's "unimplemented" list, not just the memory-related subset,
because pointers can't be declared without types/declarations, and
`Return`/`For`/`&&`/`||` are ordinary control flow any real test program is
likely to use.

Rough sizing: the currently-*working* subset of `compileOn` handles maybe a
dozen of the 48 AST node kinds meaningfully (arithmetic/comparison
expressions, `if`/`while`/`break`/`continue`, simple global-scalar
assignment, and generic calls). The remaining ~30+ node kinds needed for a
"real" C-subset program (with structs, arrays, pointers, casts, for-loops,
returns, declarations) are all unimplemented. This is a large, multi-week
front-end effort even before memory-safety instrumentation is added on top,
and it also requires deciding the VM's calling convention/entry-point
integration (item 3), which is an architecture decision, not just more
opcodes.

## 5. Memory-safety checks in the tree-walker today (baseline)

No `requireNotFreed()`-style unifying helper exists anywhere in the
repository (confirmed by a repo-wide search) — the checks remain scattered,
exactly as the task brief hedged might no longer be the case. Distinct,
independently-maintained check sites found:

- `getPointer()` (line 3202-3238): `if (get(base, Memory, free)) fatal("getting freed memory was not allowed: ...")` — gates loads through a pointer whose target `Memory` block was freed.
- `setMemory()` (line 3270-3313): `if (get(value, Pointer, isfree)) fatal("freed memory set")` — gates storing a since-freed pointer *value* into memory (different flag: `Pointer,isfree`, not `Memory,free`).
- `prim_free()` (line 2599-2619): checks `Memory,heap` (was this ever malloc'd from the heap at all) and `Memory,free` (double-free detection), then does `FREE(...)` and increments the `Memory,free` counter; separately sets `Pointer,isfree = -1` on the specific `Pointer` object passed to `free()`.
- `printiln()` (line 1439-1440): `if (get(obj, Pointer, isfree)) fatal("freed memory read")` — a debug-print path with its own ad hoc check.
- Two **commented-out** (disabled) checks were also found, both using the
  `Pointer,isfree` flag: line 3459 (`Dereference`-as-lvalue assignment path)
  and line 3805 (a dereference/eval path) — i.e. the tree-walker itself is
  not 100% consistent about enforcing use-after-free today; these are gaps
  in the baseline, not the VM.

So there are two distinct flags in play — `Memory,free` (a per-block
"freed" counter/flag on the underlying heap allocation, incremented in
`prim_free`) and `Pointer,isfree` (a per-`Pointer`-object flag set only on
the specific pointer value passed to `free()`) — checked at different call
sites with different coverage, plus at least two known disabled checks.
Any VM design should not just replicate one of these checks; it should
either reuse the exact existing call sites (via shared functions) or, ideally,
be paired with actually finishing the tree-walker's own unification (e.g. a
future `requireNotFreed()` helper) so there is truly one source of truth
both engines call into — but that unification is out of scope for the VM
work itself and doesn't currently exist.

## 6. Design proposal: enforcing memory safety in the VM path

Precondition regardless of design: **checked behavior must be the default.**
Any "unchecked/fast" mode must be an explicit, separate opt-in flag (e.g. a
hypothetical `-U`/`--unsafe`), never the default, and never silently implied
by `-O`. `-O` today already skips `typeCheck`/`preval` (§1) — that is an
existing correctness gap for `-O`, not a template to extend; the design below
assumes that gap gets closed as part of making `-O` real, not papered over.

### Option A — VM opcodes call the exact same C helpers as the tree-walker

Add pointer/memory opcodes (`iLOAD_PTR`/`iSTORE_PTR`/`iADDR_LOCAL`/
`iMALLOC`-adjacent bookkeeping, etc. — naming illustrative) whose C
implementations in `execute()` call `getPointer()`, `setMemory()`, and
(once it exists) `requireNotFreed()` directly — the identical functions
`eval()`/`assign()` call today. `compileOn` would need to lower
`Dereference`, `Index`, `Member`, and pointer-typed `Assign` into these new
opcodes instead of asserting, and the VM's value stack already happily
carries `Pointer`/`Memory`/`Struct`/`Array` `oop`s as opaque values (nothing
in `execute()`'s stack handling is type-specific), so this is representation-
compatible with today's runtime objects.

- **Pros:** Single source of truth by construction — there is exactly one
  `getPointer`/`setMemory` implementation, and both engines call it. A fix
  or a newly-added check (e.g. bounds check, taint tracking) automatically
  applies to both the tree-walker and the VM with no duplication. Minimal
  new "safety logic" surface to audit or keep in sync.
- **Cons:** Requires the VM to gain real variable/lvalue machinery first —
  `getPointer`'s `case Variable:` returns `get(base, Variable,value)`,
  i.e. it assumes locals are boxed `Variable` objects reachable the way the
  tree-walker's `Scope`/`declareVariable` produce them (§4 item 2). The VM's
  current alist-of-`(Symbol . value)` frames don't produce `Variable`
  objects, so `&local` / pointer-to-local semantics would need the VM's
  calling convention reworked to allocate real `Variable` boxes for
  parameters/locals, not just cons cells. This is extra work bundled into
  "finish the VM" regardless (declarations need *some* boxed representation),
  so it is arguably not incremental cost specific to memory-safety, but it
  does mean Option A can't be done as a narrow, self-contained patch — it's
  coupled to the declaration/scope work in §4.

### Option B — pointer ops compile down to primitive calls (`prim_*`)

Instead of new opcodes with inline C logic, `compileOn` lowers `*p`,
`p[i]`, `p->f`, `malloc(...)`, `free(...)` to `Call` nodes targeting
`Primitive` entries (`prim_deref`, `prim_index`, `prim_member`,
`prim_malloc`, `prim_free`, ...), reusing the *already-working* `iCALL`
mechanism (line 4494-4530), which already dispatches straight into
`get(func, Primitive,function)(argc, stack+sp, nil)` — no new opcode
handlers needed in `execute()` at all, only new/adapted `prim_*` C
functions (or reuse of `getPointer`/`setMemory` *inside* those primitives).

- **Pros:** Reuses the one piece of the VM that already works generically
  (`iCALL`'s `Primitive` case) and needs zero new opcodes or `execute()`
  changes — all new logic lives in ordinary C primitive functions, which is
  the same shape of code the codebase already has plenty of (`prim_malloc`,
  `prim_free`, etc.). Also naturally centralizes the check: if
  `prim_deref`/`prim_index` internally call `getPointer` (which itself calls
  a future `requireNotFreed`), there is still exactly one checked code path.
  Lower risk to the VM's core loop (no new stack-effect bugs in hand-written
  opcode cases).
- **Cons:** `Assign` through a pointer/index/member is not naturally a
  "call with a return value" in C source syntax — the compiler still needs
  lvalue-vs-rvalue codegen logic to decide *when* to emit a `prim_store*`
  call vs a `prim_load*` call, so some of Option A's front-end complexity
  (recognizing lvalue contexts) isn't actually avoided, just relocated from
  new opcode handlers into new primitive plumbing. Slightly more indirection/
  call overhead per pointer op (an `iCALL` for every dereference) versus a
  dedicated opcode, though for a research/correctness-focused interpreter
  this overhead is unlikely to matter.

### Comparison and recommendation

| | Option A (dedicated opcodes calling shared helpers) | Option B (lower to `prim_*` via `iCALL`) |
|---|---|---|
| New opcodes/`execute()` cases | Yes, several | None |
| New C surface | Opcode handlers + reused helpers | New/adapted `prim_*` functions |
| Reuses existing working VM mechanism | Partially (stack model only) | Yes (`iCALL`+`Primitive` dispatch, already implemented and tested) |
| Coupling to declaration/scope rework | Direct — needs boxed `Variable` locals | Same underlying need, but can be staged: primitives can take a `Pointer` argument value without the VM needing its *own* lvalue-to-`Variable` resolution for `&x`, since `&x` on a local can itself be compiled as a call to a `prim_addressof` that receives a `Variable` if/when one is available |
| Consistency with current VM architecture found in the code | Lower — introduces a second "opcode calls arbitrary C logic" pattern that mirrors but duplicates the shape of primitive dispatch | Higher — the existing code already treats "call out to a C function with raw stack args" (`iCALL`/`Primitive`) as the escape hatch for anything not worth a dedicated opcode; arithmetic/comparison got dedicated opcodes because they're hot and simple, but there is no precedent for giving pointer semantics dedicated opcodes yet |

**Recommendation: Option B**, with the underlying `prim_*` functions
implemented as thin wrappers that call the same `getPointer`/`setMemory`
(and a future unifying `requireNotFreed`) the tree-walker calls, giving the
benefit of Option A's "one source of truth" (§5's goal) without adding new
opcode surface to a VM whose `execute()` loop is already fairly delicate
(fixed 32-slot stacks, no growth, `fatal()` on overflow — §2). This also
matches the existing pattern in the file, where `malloc`/`free` are already
`Primitive`s reachable generically through `iCALL` (§3, `Call` case) — no VM
change was needed for those two to "work" as bare calls today, which is
itself evidence that the primitive-call route is the path of least
resistance for this codebase's actual architecture. Concretely:

1. Finish (or land, if a concurrent change does it first) a single
   `requireNotFreed(oop base)`-style helper used by `getPointer`/`setMemory`/
   `prim_free`/`printiln`, replacing the four independent scattered checks
   in §5 — this benefits the tree-walker on its own regardless of the VM.
2. Add `prim_deref`, `prim_index`, `prim_member`, `prim_store_deref`,
   `prim_store_index`, `prim_store_member` (or equivalent) primitives that
   internally call `getPointer`/`setMemory`, which in turn call
   `requireNotFreed`.
3. Extend `compileOn` to lower `Dereference`/`Index`/`Member` (rvalue
   contexts) and pointer/index/member-typed `Assign` (lvalue contexts) into
   `Call` nodes targeting those primitives, reusing the existing `Call`
   codegen and `iCALL` opcode verbatim — no `execute()` changes required.
4. Keep this checked path as the unconditional default. If an unchecked/fast
   variant is ever wanted for performance experiments, add it as a distinct,
   explicitly-named opt-in (e.g. a separate primitive set selected by a flag
   such as `-U`), never by weakening step 1's helper or making it a no-op
   under `-O`.

This keeps memory-safety enforcement centralized in one C helper reachable
from both engines, avoids adding bespoke unsafe-by-construction opcode
handlers, and defers (rather than blocks on) the larger, separate
declaration/scope rework that both options ultimately need before pointers,
arrays, or structs can be declared at all in `-O` mode (§4).

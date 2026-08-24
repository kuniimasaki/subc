typedef long intptr_t;

typedef char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef long  int64_t;

// no unsigned uint8_t/uint16_t/uint32_t/uint64_t: this language subset has
// no `unsigned` keyword at all (only the signed char/short/int/long/float/
// double/pointer/array types in _do_types), so there is no type to alias
// them to without silently changing their signedness semantics.

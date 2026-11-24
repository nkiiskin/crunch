struct FloatPair { float x, y; };
struct TinyFloat { float x; };
struct DoublePair { double x, y; };
struct IntegerSse { long i; double x; };
struct SseInteger { double x; long i; };
struct MixedWord { float x; int i; };
struct NestedFloats { struct FloatPair pair; float x; int i; };
struct FloatArray { float values[3]; };
struct BigFloats { double values[3]; };
struct FloatFlex { float x; float values[]; };
struct AlignedSI { double x; long y; long double data[]; };
struct AlignedIS { long x; double y; long double data[]; };
struct AlignedSS { double x; double y; long double data[]; };
union AlignedUnion { struct AlignedSI item; long value; };

struct FloatPair pass_float_pair(struct FloatPair);
struct TinyFloat pass_tiny_float(struct TinyFloat);
struct DoublePair pass_double_pair(struct DoublePair);
struct IntegerSse pass_integer_sse(struct IntegerSse);
struct SseInteger pass_sse_integer(struct SseInteger);
struct MixedWord pass_mixed_word(struct MixedWord);
struct NestedFloats pass_nested(struct NestedFloats);
struct FloatArray pass_array(struct FloatArray);
struct BigFloats pass_big(struct BigFloats);
struct FloatFlex pass_flexible(struct FloatFlex);
struct AlignedSI pass_aligned_si(struct AlignedSI);
struct AlignedIS pass_aligned_is(struct AlignedIS);
struct AlignedSS pass_aligned_ss(struct AlignedSS);
union AlignedUnion pass_aligned_union(union AlignedUnion);
int aligned_gp(long, long, long, long, long, long, long,
               struct AlignedSI, struct AlignedIS, long);
int aligned_fp(long, long, long, long, long, long, long,
               double, double, double, double, double, double, double, double,
               struct AlignedSS, long);
int aligned_rollback_gp(long, long, long, long, long, long, struct AlignedSI, double);
int aligned_rollback_fp(double, double, double, double, double, double, double, double,
                        struct AlignedIS, long);
int aligned_variadic_registers(int, ...);
int aligned_variadic_gp(int, ...);
int aligned_variadic_fp(int, ...);
int aligned_named_va(long, long, long, long, long, long, long, struct AlignedSI, ...);
int spill_gp(long, long, long, long, long, long, struct SseInteger, double);
int spill_fp(double, double, double, double, double, double, double, double,
             struct IntegerSse, long);
int spill_pair(double, double, double, double, double, double, double,
               struct DoublePair, double);
int read_structs(int, ...);
int read_gp_spill(long, long, long, long, long, ...);
int read_fp_spill(double, double, double, double, double, double, double, double, ...);
int check(void);

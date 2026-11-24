struct Unit { _Alignas(16) double value; };
struct Pair { _Alignas(16) int first; int second; double value; };
struct Big { char first; _Alignas(16) int value; char last; };
struct Unit crunch_unit(struct Unit);
struct Pair crunch_pair(struct Pair);
struct Big crunch_big(struct Big);

struct Unit host_unit(struct Unit value) { value.value += 10.0; return value; }
struct Pair host_pair(struct Pair value) { value.first += 10; return value; }
struct Big host_big(struct Big value) { value.value += 10; return value; }
int host_checks(void) {
    struct Unit u = {1.0};
    struct Pair p = {2, 3, 4.0};
    struct Big b = {5, 6, 7};
    u = crunch_unit(u);
    p = crunch_pair(p);
    b = crunch_big(b);
    return u.value != 3.0 || p.first != 5 || p.second != 3 || p.value != 4.0 ||
           b.first != 5 || b.value != 10 || b.last != 7;
}

extern void *__crunch_tmpfile(void);
extern unsigned long __crunch_fwrite(const void *, unsigned long, unsigned long, void *);
extern void __crunch_rewind(void *);
extern int __crunch_fclose(void *);
extern int fscanf(void *, const char *, ...);
extern int scanf(const char *, ...);

int main(void)
{
    void *stream = __crunch_tmpfile();
    int a, b, c, d, e, f, g;
    if (!stream) return 1;
    if (__crunch_fwrite("1 2 3 4 5 6 7", 1, 13, stream) != 13) return 2;
    __crunch_rewind(stream);
    if (fscanf(stream, "%d%d%d%d%d%d%d", &a, &b, &c, &d, &e, &f, &g) != 7)
        return 3;
    if (a != 1 || b != 2 || c != 3 || d != 4 || e != 5 || f != 6 || g != 7)
        return 4;
    if (__crunch_fclose(stream)) return 5;
    if (scanf("%d", &a) != 1 || a != 93) return 6;
    return 0;
}

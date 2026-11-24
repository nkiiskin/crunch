#ifndef CRUNCH_INTERNAL_VA_LIST_H
#define CRUNCH_INTERNAL_VA_LIST_H

struct __crunch_va_list_tag {
    int gp_offset;
    int fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
};

#endif

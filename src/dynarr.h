#ifndef DYNARR_H
#define DYNARR_H

#define dynarr_append(dynarr, obj, ...)                                                                  \
    do {                                                                                                 \
        typeof((dynarr)->array[0]) const s[] = { obj, __VA_ARGS__ };                                     \
        int const c = sizeof(s) / sizeof(s[0]);                                                          \
        if ((dynarr)->count + c > (dynarr)->capacity) {                                                  \
            (dynarr)->capacity = 2 * (dynarr)->count + 20;                                               \
            (dynarr)->array = realloc((dynarr)->array, (dynarr)->capacity * sizeof((dynarr)->array[0])); \
        }                                                                                                \
        memcpy(&(dynarr)->array[(dynarr)->count], &s, sizeof(s));                                        \
        (dynarr)->count += c;                                                                            \
    } while (0);

#endif // DYNARR_H

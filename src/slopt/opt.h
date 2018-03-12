// slopt - Slab Option Parser
// Copyright (c) 2014 - 2017 Luc Everse

#pragma once
#ifndef SLOPT_H_
#define SLOPT_H_
#define SLOPT_DISALLOW_ARGUMENT 0
#define SLOPT_ALLOW_ARGUMENT 1
#define SLOPT_REQUIRE_ARGUMENT 2
#ifdef __cplusplus
extern "C" {
#endif

enum slopt_Status {
    SLOPT_OK1 = '+',
    SLOPT_OK2 = '-',
    SLOPT_MISSING_ARGUMENT = '0',
    SLOPT_UNEXPECTED_ARGUMENT = '!',
    SLOPT_UNKNOWN_SHORT_OPTION = 's',
    SLOPT_UNKNOWN_LONG_OPTION = 'l',
    SLOPT_DIRECT = '>'
};
typedef struct slopt_Option {
    char shortName;
    const char *longName;
    int argument;
} slopt_Option;
typedef void (*slopt_Callback)(int sw, char snam, const char *lnam, const char *val, void *pl);
int slopt_parse(int, char **, const slopt_Option[], const slopt_Callback, void *payload);
#define SLOPT_IS_OPT(sw_) (((sw_) == SLOPT_OK1 || (sw_) == SLOPT_OK2))

#ifdef __cplusplus
}
#endif
#endif
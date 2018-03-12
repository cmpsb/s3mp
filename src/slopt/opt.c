// slopt - Slab Option Parser
// Copyright (c) 2014 - 2017 Luc Everse

#include <stdlib.h>
#include <string.h>

#include "opt.h"

static int isLong(const char * opt) {
    if(opt[0] == '\0') return 0;
    return (opt[0] == opt[1]) && ((opt[0] == '-') || (opt[0] == '+'));
}

static int isShort(const char * opt) {
    return opt[0] == '-' || opt[0] == '+';
}

static int isBreak(const char * opt) {
    return isLong(opt) && opt[2] == '\0';
}

static int isText(const char * opt) {
    return !isShort(opt);
}

static int findLongOption(const slopt_Option opts[], const char * opt) {
    for(int i = 0; opts[i].longName != NULL; ++i) {
        if(strncmp(opt, opts[i].longName, strlen(opts[i].longName)) == 0) return i;
    }

    return -1;
}

static int findShortOption(const slopt_Option opts[], char opt) {
    for(int i = 0; opts[i].shortName != '\0'; ++i) {
        if(opts[i].shortName == opt) return i;
    }

    return -1;
}

int slopt_parse(int argc, char ** argv, const slopt_Option opts[], const slopt_Callback cb, void * pl) {
    if(argv == NULL || cb == NULL) return 0;

    int i;
    for(i = 0; i < argc; ++i) {
        // -- or ++ or //, stop processing
        if(isBreak(argv[i])) return i + 1;

        // value
        if(isText(argv[i])) {
            cb(SLOPT_DIRECT, '\0', "", argv[i], pl);
            continue;
        }

        // --name
        if(isLong(argv[i])) {
            // Find the name in the option table, error if not found
            int opt = findLongOption(opts, argv[i] + 2);
            if(opt == -1) {
                // Unknown option, error
                cb(SLOPT_UNKNOWN_LONG_OPTION, '\0', argv[i] + 2, '\0', pl);
                continue;
            }

            // Locate a value separator
            char * valueStart = strpbrk(argv[i], "=:");
            if(valueStart != NULL) *valueStart = '\0';

            if(valueStart != NULL && opts[opt].argument == SLOPT_DISALLOW_ARGUMENT) {
                // No argument allowed, still given, error
                cb(SLOPT_UNEXPECTED_ARGUMENT, opts[opt].shortName, argv[i] + 2, valueStart + 1, pl);
                continue;
            }
            if(valueStart != NULL) {
                // Argument allowed, given, OK
                cb(argv[i][0], opts[opt].shortName, argv[i] + 2, valueStart + 1, pl);
                continue;
            }

            if(opts[opt].argument != SLOPT_DISALLOW_ARGUMENT) {
                if(i < argc - 1 && isText(argv[i + 1])) {
                    // Argument required or optional, given, OK
                    cb(argv[i][0], opts[opt].shortName, argv[i] + 2, argv[i + 1], pl);
                    ++i;
                    continue;
                }
                else if(opts[opt].argument == SLOPT_REQUIRE_ARGUMENT) {
                    // Argument required, none given, error
                    cb(SLOPT_MISSING_ARGUMENT, opts[opt].shortName, argv[i] + 2, "", pl);
                    continue;
                }
            }

            // No argument required, none given, OK
            cb(argv[i][0], opts[opt].shortName, argv[i] + 2, "", pl);
            continue;
        }

        // -s
        if(isShort(argv[i])) {
            for(size_t j = 1; argv[i][j] != '\0' && argv[i][j] != '=' && argv[i][j] != ':'; ++j) {
                // Find the switch in the option table, error if not found
                int opt = findShortOption(opts, argv[i][j]);
                if(opt == -1) {
                    // Unknown option, error
                    cb(SLOPT_UNKNOWN_SHORT_OPTION, argv[i][j], "", "", pl);
                    continue;
                }

                if((argv[i][j + 1] == '=' || argv[i][j + 1] == ':')
                   && opts[opt].argument != SLOPT_DISALLOW_ARGUMENT) {
                    // Argument required or optional, given, OK
                    cb(argv[i][0], argv[i][j], "", argv[i] + j + 2, pl);
                    break;
                }
                else if(argv[i][j + 1] == '=' || argv[i][j + 1] == ':') {
                    // Argument disallowed, still given, error
                    cb(SLOPT_UNEXPECTED_ARGUMENT, argv[i][j], opts[opt].longName,
                       argv[i] + j + 2, pl);
                    break;
                }

                if(argv[i][j + 1] == '\0' && opts[opt].argument != SLOPT_DISALLOW_ARGUMENT
                   && i < argc - 1 && isText(argv[i + 1])) {
                    // End of switch collection plus value in next slot
                    cb(argv[i][0], argv[i][j], opts[opt].longName, argv[i + 1], pl);
                    ++i;
                    break;
                }

                if(opts[opt].argument == SLOPT_REQUIRE_ARGUMENT) {
                    // Argument required, none given, error
                    cb(SLOPT_MISSING_ARGUMENT, argv[i][j], opts[opt].longName, "", pl);
                    continue;
                }

                cb(argv[i][0], argv[i][j], opts[opt].longName, "", pl);
                continue;
            }
        }
    }

    return i;
}
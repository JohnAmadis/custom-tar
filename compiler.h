#ifndef COMPILER_H
#define COMPILER_H

#if defined(__GNUC__) || defined(__clang__)
#   define PACKED __attribute__((__packed__))
#else 
#   error "Compiler not supported"
#endif

#endif // COMPILER_H
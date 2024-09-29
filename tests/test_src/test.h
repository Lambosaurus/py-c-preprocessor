#ifndef TEST_H
#define TEST_H

/*
 * PUBLIC DEFINITIONS
 */

#define MACRO_CONST         0x1

#ifdef MACRO_CONST
#define MACRO_A(a,b)        		(a + b)
#define MACRO_B(a)          		(a + MACRO_CONST)
#define MACRO_C(a,b)        		(MACRO_A(a, 1) + MACRO_B(b))
#define MACRO_D(v)          		(v & (512 - 1))
#else
#error "This clause should not be reached"
#endif

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

/*
 * EXTERN DECLARATIONS
 */

#endif //TEST_H

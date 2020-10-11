#ifndef _SAVE_MACROS_INCLUDED
#define _SAVE_MACROS_INCLUDED

#define MIN(a, b) ({ \
	__auto_type _a = a; __auto_type _b = b; \
	_a < _b? _a : _b; \
})

#endif

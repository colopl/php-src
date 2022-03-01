/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Sascha Schumann <sascha@schumann.cx>                        |
   |          Pedro Melo <melo@ip.pt>                                     |
   |          Sterling Hughes <sterling@php.net>                          |
   |          Sammy Kaye Powers <me@sammyk.me>                            |
   |          Go Kudo <g-kudo@colopl.co.jp>                               |
   |                                                                      |
   | Based on code from: Richard J. Wagner <rjwagner@writeme.com>         |
   |                     Makoto Matsumoto <matumoto@math.keio.ac.jp>      |
   |                     Takuji Nishimura                                 |
   |                     Shawn Cokus <Cokus@math.washington.edu>          |
   |                     David Blackman                                   |
   |                     Sebastiano Vigna <vigna@acm.org>                 |
   |                     Melissa O'Neill <oneill@pcg-random.org>          |
   +----------------------------------------------------------------------+
*/
#ifndef PHP_RANDOM_H
# define PHP_RANDOM_H

/* System Rand functions */
# ifndef RAND_MAX
#  define RAND_MAX PHP_MT_RAND_MAX
# endif

# define PHP_RAND_MAX PHP_MT_RAND_MAX

/*
 * A bit of tricky math here.  We want to avoid using a modulus because
 * that simply tosses the high-order bits and might skew the distribution
 * of random values over the range.  Instead we map the range directly.
 *
 * We need to map the range from 0...M evenly to the range a...b
 * Let n = the random number and n' = the mapped random number
 *
 * Then we have: n' = a + n(b-a)/M
 *
 * We have a problem here in that only n==M will get mapped to b which
 * means the chances of getting b is much much less than getting any of
 * the other values in the range.  We can fix this by increasing our range
 * artificially and using:
 *
 *               n' = a + n(b-a+1)/M
 *
 * Now we only have a problem if n==M which would cause us to produce a
 * number of b+1 which would be bad.  So we bump M up by one to make sure
 * this will never happen, and the final algorithm looks like this:
 *
 *               n' = a + n(b-a+1)/(M+1)
 *
 * -RL
 */
# define RAND_RANGE_BADSCALING(__n, __min, __max, __tmax) \
	(__n) = (__min) + (zend_long) ((double) ( (double) (__max) - (__min) + 1.0) * ((__n) / ((__tmax) + 1.0)))

# ifdef PHP_WIN32
#  define GENERATE_SEED() (((zend_long) (time(0) * GetCurrentProcessId())) ^ ((zend_long) (1000000.0 * php_combined_lcg())))
# else
#  define GENERATE_SEED() (((zend_long) (time(0) * getpid())) ^ ((zend_long) (1000000.0 * php_combined_lcg())))
# endif

# define PHP_MT_RAND_MAX ((zend_long) (0x7FFFFFFF)) /* (1<<31) - 1 */

# define MT_RAND_MT19937 0
# define MT_RAND_PHP 1

# define MT_N (624)

# define php_random_bytes_throw(b, s) php_random_bytes((b), (s), 1)
# define php_random_bytes_silent(b, s) php_random_bytes((b), (s), 0)

# define php_random_int_throw(min, max, result) \
	php_random_int((min), (max), (result), 1)
# define php_random_int_silent(min, max, result) \
	php_random_int((min), (max), (result), 0)

# if !defined(__SIZEOF_INT128__) || defined(FORCE_EMULATE_128)
#  define RANDOM_PCG64_EMULATED
typedef struct _random_uint128_t {
	uint64_t hi;
	uint64_t lo;
} random_uint128_t;
#  define UINT128_HI64(value)	value.hi;
#  define UINT128_LO64(value)	value.lo;
#  define UINT128_CON(x, y, result)	\
	do { \
		result.hi = x; \
		result.lo = y; \
	} while (0);
#  define UINT128_ADD(x, y, result)	\
	do { \
		const uint64_t _lo = (x.lo + y.lo), _hi = (x.hi + y.hi + (_lo < x.lo)); \
		result.hi = _hi; \
		result.lo = _lo; \
	} while (0);
#  define UINT128_MUL(x, y, result)	\
	do { \
		const uint64_t \
			_x0 = x.lo & 0xffffffffULL, \
			_x1 = x.lo >> 32, \
			_y0 = y.lo & 0xffffffffULL, \
			_y1 = y.lo >> 32, \
			_z0 = ((_x1 * _y0 + (_x0 * _y0 >> 32)) & 0xffffffffULL) + _x0 * _y1; \
		result.hi = x.hi * y.lo + x.lo * y.hi; \
		result.lo = x.lo * y.lo; \
		result.hi += _x1 * _y1 + ((_x1 * _y0 + (_x0 * _y0 >> 32)) >> 32) + (_z0 >> 32); \
	} while (0);
#  define PCG64_ROTL1OR1(x, result)	\
	do { \
		result.hi = x.hi << 1U | x.lo >> 63U; \
		result.lo = x.lo << 1U | 1U; \
	} while (0);
#  define PCG64_ROTR64(x, result)	\
	do { \
		const uint64_t _v = (x.hi ^ x.lo), _s = x.hi >> 58U; \
		result = (_v >> _s) | (_v << ((-_s) & 63)); \
	} while (0);
# else
typedef __uint128_t random_uint128_t;
#  define UINT128_HI64(value)	(uint64_t) (value >> 64);
#  define UINT128_LO64(value)	(uint64_t) value;
#  define UINT128_CON(x, y, result)	result = (((random_uint128_t) x << 64) + y);
#  define UINT128_ADD(x, y, result)	result = x + y;
#  define UINT128_MUL(x, y, result)	result = x * y;
#  define PCG64_ROTL1OR1(x, result)	result = (x << 1U) | 1U;
#  define PCG64_ROTR64(x, result)	\
	do { \
		uint64_t _v = ((uint64_t) (x >> 64U)) ^ (uint64_t) x, _s = x >> 122U; \
		result = (_v >> _s) | (_v << ((-_s) & 63)); \
	} while (0);
# endif

# define RANDOM_ENGINE_GENERATE(algo, state, result, generated_size, rng_unsafe)	\
	do { \
		result = algo->generate(state, rng_unsafe); \
		generated_size = algo->static_generate_size != 0 \
			? algo->static_generate_size \
			: algo->dynamic_generate_size(state) \
		; \
	} while (0);
# define RANDOM_ENGINE_GENERATE_SILENT(algo, state, result, generate_size)	RANDOM_ENGINE_GENERATE(algo, state, result, generated_size, NULL)

PHPAPI double php_combined_lcg(void);

PHPAPI void php_srand(zend_long seed);
PHPAPI zend_long php_rand(void);

PHPAPI void php_mt_srand(uint32_t seed);
PHPAPI uint32_t php_mt_rand(void);
PHPAPI zend_long php_mt_rand_range(zend_long min, zend_long max);
PHPAPI zend_long php_mt_rand_common(zend_long min, zend_long max);

PHPAPI int php_random_bytes(void *bytes, size_t size, bool should_throw);
PHPAPI int php_random_int(zend_long min, zend_long max, zend_long *result, bool should_throw);

typedef struct _php_random_engine_algo {
	const size_t static_generate_size;						/* Specifies the generated size. if the generated size is to be changed dynamically to set 0. */
	size_t (*dynamic_generate_size)(void *state);			/* Pointer to get the dynamic generation size. non-nullable. */
	const size_t state_size;								/* Size of the structure to hold the state. if not needed to set 0. */
	uint64_t (*generate)(void *state, bool *rng_unsafe);	/* Pointer to get random number. non-nullable. */
	void (*seed)(void *state, const uint64_t seed);			/* Pointer to seeding state. nullable. */
	int (*serialize)(void *state, HashTable *data);			/* Pointer to serialize state. nullable. */
	int (*unserialize)(void *state, HashTable *data);		/* Pointer to unserialize state. nullable. */
} php_random_engine_algo;

PHPAPI const php_random_engine_algo *php_random_engine_get_default_algo(void);
PHPAPI void *php_random_engine_get_default_state(void);

PHPAPI zend_long php_random_engine_range(const php_random_engine_algo *algo, void *state, zend_long min, zend_long max, bool *rng_unsafe);

extern zend_module_entry random_module_entry;
# define phpext_random_ptr &random_module_entry

extern PHPAPI zend_class_entry *random_ce_Random_Engine;
extern PHPAPI zend_class_entry *random_ce_Random_CryptoSafeEngine;
extern PHPAPI zend_class_entry *random_ce_Random_SeedableEngine;
extern PHPAPI zend_class_entry *random_ce_Random_SerializableEngine;

extern PHPAPI zend_class_entry *random_ce_Random_Engine_CombinedLCG;
extern PHPAPI zend_class_entry *random_ce_Random_Engine_PCG64;
extern PHPAPI zend_class_entry *random_ce_Random_Engine_MersenneTwister;
extern PHPAPI zend_class_entry *random_ce_Random_Engine_Secure;
extern PHPAPI zend_class_entry *random_ce_Random_Engine_XorShift128Plus;
extern PHPAPI zend_class_entry *random_ce_Random_Engine_Xoshiro256StarStar;
extern PHPAPI zend_class_entry *random_ce_Random_Randomizer;

extern const php_random_engine_algo php_random_engine_algo_combinedlcg;
extern const php_random_engine_algo php_random_engine_algo_mersennetwister;
extern const php_random_engine_algo php_random_engine_algo_pcg64;
extern const php_random_engine_algo php_random_engine_algo_secure;
extern const php_random_engine_algo php_random_engine_algo_user;
extern const php_random_engine_algo php_random_engine_algo_xorshift128plus;
extern const php_random_engine_algo php_random_engine_algo_xoshiro256starstar;

typedef struct _php_random_engine {
	const php_random_engine_algo *algo;
	void *state;
	zend_object std;
} php_random_engine;

typedef struct _php_random_engine_state_combinedlcg {
	int32_t s[2];
	bool seeded;
} php_random_engine_state_combinedlcg;

typedef struct _php_random_engine_state_mersennetwister {
	uint32_t s[MT_N];
	int cnt;
	zend_long mode;
	bool seeded;
} php_random_engine_state_mersennetwister;

typedef struct _php_random_engine_state_pcg64 {
	random_uint128_t s;
	random_uint128_t inc;
} php_random_engine_state_pcg64;

typedef struct _php_random_engine_state_user {
	zend_object *object;
	zend_function *generate_method;
	size_t last_generate_size;
} php_random_engine_state_user;

typedef struct _php_random_engine_state_xorshift128plus {
	uint64_t s[2];
} php_random_engine_state_xorshift128plus;

typedef struct _php_random_engine_state_xoshiro256starstar {
	uint64_t s[4];
} php_random_engine_state_xoshiro256starstar;

typedef struct _php_random_randomizer {
	const php_random_engine_algo *algo;
	void *state;
	bool self_allocate;
	zend_object std;
} php_random_randomizer;

static inline php_random_engine *php_random_engine_from_obj(zend_object *obj) {
	return (php_random_engine *)((char *)(obj) - XtOffsetOf(php_random_engine, std));
}

static inline php_random_randomizer *php_random_randomizer_from_obj(zend_object *obj) {
	return (php_random_randomizer *)((char *)(obj) - XtOffsetOf(php_random_randomizer, std));
}

# define Z_RANDOM_ENGINE_P(zval) php_random_engine_from_obj(Z_OBJ_P(zval))

# define Z_RANDOM_RANDOMIZER_P(zval) php_random_randomizer_from_obj(Z_OBJ_P(zval))

PHP_MINIT_FUNCTION(random);
PHP_MSHUTDOWN_FUNCTION(random);
PHP_RINIT_FUNCTION(random);

ZEND_BEGIN_MODULE_GLOBALS(random)
	php_random_engine_state_combinedlcg clcg;		/* Combined LCG global state */
	php_random_engine_state_mersennetwister mt;		/* MersenneTwister global state */
	int random_fd;									/* random file discriptor */
ZEND_END_MODULE_GLOBALS(random)

# define RANDOM_G(v)	ZEND_MODULE_GLOBALS_ACCESSOR(random, v)

#endif	/* PHP_RANDOM_H */

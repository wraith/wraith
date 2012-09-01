/*
 * Macros to pull sec and nsec parts of mtime from struct stat.
 */
#ifdef HAVE_ST_MTIM
# define mtim_getsec(_x)        ((_x).st_mtim.tv_sec)
# define mtim_getnsec(_x)       ((_x).st_mtim.tv_nsec)
#else
# ifdef HAVE_ST_MTIMESPEC
#  define mtim_getsec(_x)       ((_x).st_mtimespec.tv_sec)
#  define mtim_getnsec(_x)      ((_x).st_mtimespec.tv_nsec)
# else
#  define mtim_getsec(_x)       ((_x).st_mtime)
#  define mtim_getnsec(_x)      (0)
# endif /* HAVE_ST_MTIMESPEC */
#endif /* HAVE_ST_MTIM */

/*
 * Enable IPv6 debugging?
 */
#define DEBUG_IPV6 1
#define HAVE_IPV6 1

/* IPv6 sanity checks. */
#ifdef USE_IPV6
#  ifndef HAVE_IPV6
#    undef USE_IPV6
#  endif
#endif

/* TCL sanity check */
#ifdef USE_SCRIPT_TCL
#  ifndef HAVE_LIBTCL
#    undef USE_SCRIPT_TCL
#  endif
#endif


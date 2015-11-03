/* libc uncompressing functions for dns answers
 *
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * Define constants based on RFC 883, RFC 1034, RFC 1035
 */
#define NS_PACKETSZ     512     /* maximum packet size */
#define NS_MAXDNAME     1025    /* maximum domain name */
#define NS_MAXCDNAME    255     /* maximum compressed domain name */
#define NS_MAXLABEL     63      /* maximum length of domain label */
#define NS_HFIXEDSZ     12      /* #/bytes of fixed data in header */
#define NS_QFIXEDSZ     4       /* #/bytes of fixed data in query */
#define NS_RRFIXEDSZ    10      /* #/bytes of fixed data in r record */
#define NS_INT32SZ      4       /* #/bytes of data in a uint32_t */
#define NS_INT16SZ      2       /* #/bytes of data in a uint16_t */
#define NS_INT8SZ       1       /* #/bytes of data in a uint8_t */
#define NS_INADDRSZ     4       /* IPv4 T_A */
#define NS_IN6ADDRSZ    16      /* IPv6 T_AAAA */
#define NS_CMPRSFLGS    0xc0    /* Flag bits indicating name compression. */
#define NS_DEFAULTPORT  53      /* For both TCP and UDP. */


static const char       digits[] = "0123456789";

/*
 * special(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this characted special ("in need of quoting") ?
 * return:
 *      boolean.
 */
static inline int special(int ch) {
        switch (ch) {
        case 0x22: /* '"' */
        case 0x2E: /* '.' */
        case 0x3B: /* ';' */
        case 0x5C: /* '\\' */
        /* Special modifiers in zone files. */
        case 0x40: /* '@' */
        case 0x24: /* '$' */
                return (1);
        default:
                return (0);
        }
}

/*
 * printable(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this character visible and not a space when printed ?
 * return:
 *      boolean.
 */
static inline int printable(int ch) {
        return (ch > 0x20 && ch < 0x7f);
}

/*
 * ns_name_ntop(src, dst, dstsiz)
 *      Convert an encoded domain name to printable ascii as per RFC1035.
 * return:
 *      Number of bytes written to buffer, or -1 (with errno set)
 * notes:
 *      The root is returned as "."
 *      All other domains are returned in non absolute form
 */

static int my_ns_name_ntop(const unsigned char *src, char *dst, size_t dstsiz) {
        const unsigned char *cp;
        char *dn, *eom;
        unsigned char c, n;

        cp = src;
        dn = dst;
        eom = dst + dstsiz;

        while ((n = *cp++) != 0) {
                if ((n & NS_CMPRSFLGS) != 0) {
                        /* Some kind of compression pointer. */
                        errno = EMSGSIZE;
                        return (-1);
                }
                if (dn != dst) {
                        if (dn >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        *dn++ = '.';
                }
                if (dn + n >= eom) {
                        errno = EMSGSIZE;
                        return (-1);
                }
                for ((void)NULL; n > 0; n--) {
                        c = *cp++;
                        if (special(c)) {
                                if (dn + 1 >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = '\\';
                                *dn++ = (char)c;
                        } else if (!printable(c)) {
                                if (dn + 3 >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = '\\';
                                *dn++ = digits[c / 100];
                                *dn++ = digits[(c % 100) / 10];
                                *dn++ = digits[c % 10];
                        } else {
                                if (dn >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = (char)c;
                        }
                }
        }
        if (dn == dst) {
                if (dn >= eom) {
                        errno = EMSGSIZE;
                        return (-1);
                }
                *dn++ = '.';
        }
        if (dn >= eom) {
                errno = EMSGSIZE;
                return (-1);
        }
        *dn++ = '\0';
        return (dn - dst);
}

/*
 * ns_name_unpack(msg, eom, src, dst, dstsiz)
 *      Unpack a domain name from a message, source may be compressed.
 * return:
 *      -1 if it fails, or consumed octets if it succeeds.
 */

static int my_ns_name_unpack(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, unsigned char *dst, size_t dstsiz)
{
        const unsigned char *srcp, *dstlim;
        unsigned char *dstp;
        int n, len, checked;

        len = -1;
        checked = 0;
        dstp = dst;
        srcp = src;
        dstlim = dst + dstsiz;
        if (srcp < msg || srcp >= eom) {
                errno = EMSGSIZE;
                return (-1);
        }
        /* Fetch next label in domain name. */
        while ((n = *srcp++) != 0) {
                /* Check for indirection. */
                switch (n & NS_CMPRSFLGS) {
                case 0:
                        /* Limit checks. */
                        if (dstp + n + 1 >= dstlim || srcp + n >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        checked += n + 1;
                        *dstp++ = n;
                        memcpy(dstp, srcp, n);
                        dstp += n;
                        srcp += n;
                        break;

                case NS_CMPRSFLGS:
                        if (srcp >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        if (len < 0)
                                len = srcp - src + 1;
                        srcp = msg + (((n & 0x3f) << 8) | (*srcp & 0xff));
                        if (srcp < msg || srcp >= eom) {  /* Out of range. */
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        checked += 2;
                        /*
                         * Check for loops in the compressed name;
                         * if we've looked at the whole message,
                         * there must be a loop.
                         */
                        if (checked >= eom - msg) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        break;

                default:
                        errno = EMSGSIZE;
                        return (-1);                    /* flag error */
                }
        }
        *dstp = '\0';
        if (len < 0)
                len = srcp - src;
        return (len);
}

/*
 * ns_name_uncompress(msg, eom, src, dst, dstsiz)
 *      Expand compressed domain name to presentation format.
 * return:
 *      Number of bytes read out of `src', or -1 (with errno set).
 * note:
 *      Root domain returns as "." not "".
 */

static int my_ns_name_uncompress(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, char *dst, size_t dstsiz)
{
        unsigned char tmp[NS_MAXCDNAME];
        int n;

        if ((n = my_ns_name_unpack(msg, eom, src, tmp, sizeof tmp)) == -1)
                return (-1);
        if (my_ns_name_ntop(tmp, dst, dstsiz) == -1)
                return (-1);
        return (n);
}

/*
 * Expand compressed domain name 'comp_dn' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eomorig' points to the first location after the message,
 * 'exp_dn' is a pointer to a buffer of size 'length' for the result.
 * Return size of compressed name or -1 if there was an error.
 */
int my_dn_expand(const unsigned char *msg, const unsigned char *eom, const unsigned char *src, char *dst, int dstsiz)
{
        int n = my_ns_name_uncompress(msg, eom, src, dst, (size_t)dstsiz);

        if (n > 0 && dst[0] == '.')
                dst[0] = '\0';
        return (n);
}

/* vim: set sts=2 sw=2 ts=8 et: */

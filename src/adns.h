/* new async dns
 *
 */

#ifndef _ADNS_H_
#define _ADNS_H_

/* RFC1035
TYPE            value and meaning
A               1 a host address
NS              2 an authoritative name server
MD              3 a mail destination (Obsolete - use MX)
MF              4 a mail forwarder (Obsolete - use MX)
CNAME           5 the canonical name for an alias
SOA             6 marks the start of a zone of authority
MB              7 a mailbox domain name (EXPERIMENTAL)
MG              8 a mail group member (EXPERIMENTAL)
MR              9 a mail rename domain name (EXPERIMENTAL)
NULL            10 a null RR (EXPERIMENTAL)
WKS             11 a well known service description
PTR             12 a domain name pointer
HINFO           13 host information
MINFO           14 mailbox or mail list information
MX              15 mail exchange
TXT             16 text strings
*/

#define DNS_A		1
#define DNS_CNAME	5
#define DNS_PTR		12
#define DNS_AAAA	28

#define DNS_LOOKUP_A    1
#define DNS_LOOKUP_AAAA 2


#define DNS_IPV4	1
#define DNS_IPV6	2
#define DNS_REVERSE	3

#define DNS_PORT 53

#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

typedef void (*dns_callback_t)(int, void *client_data, const char *query,
    const bd::Array<bd::String>& answers);

int egg_dns_init(void);
//int egg_dns_shutdown(void);

int egg_dns_lookup(const char *host, interval_t timeout, dns_callback_t callback, void *client_data, int type = (DNS_LOOKUP_A|DNS_LOOKUP_AAAA));
bd::Array<bd::String> dns_lookup_block(const char *host, interval_t timeout, int type = (DNS_LOOKUP_A|DNS_LOOKUP_AAAA));
int egg_dns_reverse(const char *ip, interval_t timeout, dns_callback_t callback, void *client_data);
bd::Array<bd::String> dns_reverse_block(const char *ip, interval_t timeout);
int egg_dns_cancel(int id, int issue_callback);
void tell_dnsdebug(int);
void dns_cache_flush();
bool valid_dns_id(int, int);
int reverse_ip(const char *host, char *reverse);
bd::String dns_find_ip(const bd::Array<bd::String>& ips, int af_type);

extern int		dns_sock, dns_idx;
extern const char	*dns_ip;
#endif /* !_EGG_DNS_H_ */

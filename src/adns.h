#ifndef _ADNS_H_
#define _ADNS_H_

#define DNS_IPV4	1
#define DNS_IPV6	2
#define DNS_REVERSE	3

#define DNS_PORT 53

typedef void (*dns_callback_t)(int id, void *client_data, const char *query, char **result);

int egg_dns_init(void);
//int egg_dns_shutdown(void);

void egg_dns_send(char *query, int len);
int egg_dns_lookup(const char *host, int timeout, dns_callback_t callback, void *client_data);
int egg_dns_reverse(const char *ip, int timeout, dns_callback_t callback, void *client_data);
int egg_dns_cancel(int id, int issue_callback);
void tell_dnsdebug(int);
void dns_cache_flush();
bool valid_dns_id(int, int);

extern int		dns_sock, dns_idx;
extern const char	*dns_ip;
#endif /* !_EGG_DNS_H_ */

#ifndef _SETTINGS_H
#define _SETTINGS_H

char *progname();
void init_settings();

extern char			packname[], shellhash[], bdhash[], dcc_prefix[],
				*owners, *hubs, *owneremail;

#endif /* !_SETTINGS_H */

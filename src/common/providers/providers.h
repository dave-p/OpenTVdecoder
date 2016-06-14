#ifndef _OPENTV_PROVIDERS_H_
#define _OPENTV_PROVIDERS_H_

int  *providers_get_channels_pids ();
int  *providers_get_titles_pids ();
int  *providers_get_summaries_pids ();
int  *providers_get_channels_types ();
int  providers_get_channels_pids_count ();
int  providers_get_titles_pids_count ();
int  providers_get_summaries_pids_count ();
int  providers_get_channels_types_count ();
int  providers_get_nid ();
int  providers_get_tsid ();
int  providers_get_sid ();
int  providers_get_protocol ();
bool providers_read (char *read);

#endif // _OPENTV_PROVIDERS_H_

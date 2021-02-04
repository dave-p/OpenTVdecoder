#ifndef _COMMON_H_
#define _COMMON_H_

typedef enum type_bool {false = 0, true} bool;

#define MAX_OTV_LOOP_CYCLES	90000
#define MAX_BAT_LOOP_CYCLES	200

#define DEFAULT_OTV_PROVIDER	"skyuk_28.2"

#define DEFAULT_DB_ROOT		"/tmp/xmltv"

#define DEFAULT_DEMUXER		"/dev/dvb/adapter0/demux0"

extern char provider[256];
extern bool no_dvb_poll;
extern bool carousel_dvb_poll;
extern bool free_only;
extern FILE *outfile;

//#define MEMORYMON true

#ifdef MEMORYMON
void *_malloc (unsigned int size);
void _free (void *value);
void memory_stats ();
void print_meminfo ();
#else
#define _malloc malloc
#define _free free
static inline void memory_stats() {}
static inline void print_meminfo() {}
#endif
#endif // _COMMON_H_

#ifndef TUNDRA_ENGINE_H
#define TUNDRA_ENGINE_H

#include <stddef.h>
#include <time.h>

#define TUNDRA_ENGINE_MTNAME "tundra_engine"
#define TUNDRA_NODEREF_MTNAME "tundra_noderef"

struct td_engine;
struct td_file;

typedef struct td_digest {
	unsigned char data[16];
} td_digest;

struct lua_State;

enum
{
	TD_PASS_MAX = 32
};

typedef void (*td_sign_fn)(struct td_engine *engine, struct td_file *f, td_digest *out);

typedef struct td_signer
{
	int is_lua;
	union {
		td_sign_fn function;
		int lua_reference;
	} function;
} td_signer;

enum {
	TD_STAT_DIR = 1 << 0,
	TD_STAT_EXISTS = 1 << 1
};

typedef struct td_stat {
	int flags;
	unsigned long long size;
	unsigned long timestamp;
} td_stat;

typedef struct td_file
{
	struct td_file *bucket_next;

	unsigned int hash;
	const char *path;
	const char *name; /* points into path */
	int path_len; /* # characters in path string */

	struct td_node *producer;
	td_signer* signer;

	int signature_dirty;
	td_digest signature;

	int stat_dirty;
	struct td_stat stat;
} td_file;

typedef enum td_jobstate
{
	TD_JOB_INITIAL         = 0,
	TD_JOB_BLOCKED         = 1,
	TD_JOB_SCANNING        = 2,
	TD_JOB_RUNNING         = 3,
	TD_JOB_COMPLETED       = 100,
	TD_JOB_FAILED          = 101,
	TD_JOB_CANCELLED       = 102,
	TD_JOB_UPTODATE        = 103
} td_jobstate;

typedef struct td_job_chain
{
	struct td_node *node;
	struct td_job_chain *next;
} td_job_chain;

enum
{
	TD_JOBF_QUEUED		= 1 << 0,
	TD_JOBF_ROOT		= 1 << 1,
	TD_JOBF_ANCESTOR_UPDATED = 1 << 16
};


typedef struct td_job 
{
	int flags;
	td_jobstate state;
	struct td_node *node;

	/* implicit dependencies, discovered by the node's scanner */
	int idep_count;
	td_file **ideps;

	/* # of jobs that must complete before this job can run */
	int block_count;

	/* # of dependencies that have failed */
	int failed_deps;

	/* list of jobs this job will unblock once completed */
	td_job_chain *pending_jobs;

	td_digest input_signature;
} td_job;

typedef struct td_ancestor_data
{
	td_digest guid;
	td_digest input_signature;
	int job_result;
	time_t access_time;
} td_ancestor_data;

typedef struct td_node
{
	const char *annotation;
	const char *action;

	int input_count;
	td_file **inputs;

	int output_count;
	td_file **outputs;

	int pass_index;

	struct td_scanner *scanner;

	int dep_count;
	struct td_node **deps;

	td_digest guid;
	const td_ancestor_data *ancestor_data;

	td_job job;
} td_node;

typedef struct td_noderef
{
	td_node *node;
} td_noderef;

typedef struct td_pass
{
	const char *name;
	int build_order;
	td_node *barrier_node;
	int node_count;
	td_job_chain *nodes;
} td_pass;

typedef struct td_alloc
{
	/* memory allocation */
	int page_index;
	int page_left;
	int page_size;
	int total_page_count;
	char **pages;
} td_alloc;

/* Caches a relation between a file and set of other files (such as set of
 * included files) */
typedef struct td_relcell
{
	/* source file */
	td_file *file;

	/* a salt value to make this relation unique */
	unsigned int salt;

	/* the related files */
	int count;
	td_file **files;

	/* for hash table linked list maintenance */
	struct td_relcell *bucket_next;
} td_relcell;

enum
{
	TD_DEBUG_QUEUE = 1 << 0,
	TD_DEBUG_NODES = 1 << 1,
	TD_DEBUG_ANCESTORS = 1 << 2,
	TD_DEBUG_STATS = 1 << 3,
	TD_DEBUG_REASON = 1 << 4,
	TD_DEBUG_SCAN = 1 << 5
};

typedef struct td_engine
{
	int magic_value;

	/* memory allocation */
	td_alloc alloc;

	/* Some string e.g. "win32-release". Used to salt node guids. */
	const char *build_id;

	/* file db */
	int file_hash_size;
	td_file **file_hash;

	/* file relation cache */
	int relhash_size;
	td_relcell **relhash;

	/* build passes */
	int pass_count;
	td_pass passes[TD_PASS_MAX];

	td_signer *default_signer;

	int node_count;

	struct {
		int verbosity;
		int debug_flags;
		int thread_count;
	} settings;

	struct {
		int file_count;
		int stat_calls;
		int stat_checks;
		int ancestor_checks;
		int ancestor_nodes;
		int md5_sign_count;
		int timestamp_sign_count;
		double scan_time;
		double build_time;
		double mkdir_time;
		double stat_time;
		double up2date_check_time;
		double file_signing_time;
	} stats;

	int ancestor_count;
	struct td_ancestor_data *ancestors;

	struct lua_State *L;
} td_engine;

#define td_verbosity_check(engine, level) ((engine)->settings.verbosity >= (level))
#define td_debug_check(engine, flags) ((engine)->settings.debug_flags & (flags))

#define td_check_noderef(L, index) ((struct td_noderef *) luaL_checkudata(L, index, TUNDRA_NODEREF_MTNAME))
#define td_check_engine(L, index) ((struct td_engine *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

void *td_page_alloc(td_alloc *engine, size_t size);
td_file *td_engine_get_file(td_engine *engine, const char *path);

td_file **
td_engine_get_relations(td_engine *engine, td_file *file, unsigned int salt, int *count_out);

void
td_engine_set_relations(td_engine *engine, td_file *file, unsigned int salt, int count, td_file **files);

const td_stat* td_stat_file(td_engine *engine, td_file *f);
void td_touch_file(td_file *f);
td_digest *td_get_signature(td_engine *engine, td_file *f);

td_file *td_parent_dir(td_engine *engine, td_file *f);

#endif
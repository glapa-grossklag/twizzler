#pragma once

#include <lib/list.h>
#include <twz/sys/kso.h>

struct thread;
struct sctx;

struct kso_view {
	struct list contexts;
	int init;
};

struct kso_throbj {
	struct thread *thread;
};

struct kso_sctx {
	struct sctx *sc;
};

struct kso_invl_args {
	objid_t id;
	uint64_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
};

struct object;
struct kso_calls {
	bool (*attach)(struct object *parent, struct object *child, int flags);
	bool (*detach)(struct object *parent, struct object *child, int sysc, int flags);
	bool (*detach_event)(struct thread *thr, bool, int);
	void (*ctor)(struct object *);
	void (*dtor)(struct object *);
	bool (*invl)(struct object *, struct kso_invl_args *);
};

void kso_register(int t, struct kso_calls *);
void kso_detach_event(struct thread *thr, bool entry, int sysc);
int kso_root_attach(struct object *obj, uint64_t flags, int type);
void kso_root_detach(int idx);
void kso_attach(struct object *parent, struct object *child, size_t);
void kso_setname(struct object *obj, const char *name);
struct object *get_system_object(void);
void obj_kso_init(struct object *, enum kso_type);
struct kso_calls *kso_lookup_calls(enum kso_type ksot);

void object_init_kso_data(struct object *, enum kso_type);
void *object_get_kso_data_checked(struct object *obj, enum kso_type kt);

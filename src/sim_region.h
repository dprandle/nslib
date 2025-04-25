#pragma once

#include "math/matrix4.h"
#include "model.h"

namespace nslib
{
struct submesh;
enum comp_type
{
    COMP_TYPE_TRANSFORM,
    COMP_TYPE_CAMERA,
    COMP_TYPE_STATIC_MODEL,
    COMP_TYPE_USER
};

enum comp_flags : u64
{
    COMP_FLAG_DIRTY = 1
};

#define COMP(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = COMP_TYPE_##type;                                                                                 \
    u32 ent_id;                                                                                                                            \
    u64 flags;

struct transform
{
    COMP(TRANSFORM)
    mat4 cached;
    vec3 world_pos;
    quat orientation;
    vec3 scale{1};
};

struct static_model
{
    COMP(STATIC_MODEL)
    rid mesh_id;
    static_array<rid, MAX_SUBMESH_COUNT> mat_ids{{}, MAX_SUBMESH_COUNT};
};

struct camera
{
    COMP(CAMERA)
    mat4 proj;
    mat4 view;
};

template<class T>
struct comp_table
{
    array<T> entries;
    hmap<u32, sizet> entc_hm;
};

struct comp_db
{
    array<void *> comp_tables;
};

struct entity
{
    u32 id;
    string name;
    comp_db *cdb;
};

struct sim_region
{
    array<entity> ents;
    hmap<u32, sizet> entmap;
    comp_db cdb;
    u32 last_id{};
};

template<class T>
void init_comp_tbl(comp_table<T> *tbl, mem_arena *arena, sizet initial_capacity, sizet mem_alignment)
{
    arr_init(&tbl->entries, arena, initial_capacity, mem_alignment);
    hmap_init(&tbl->entc_hm, hash_type, arena);
}

template<class T>
void terminate_comp_tbl(comp_table<T> *tbl)
{
    hmap_terminate(&tbl->entc_hm);
    arr_terminate(&tbl->entries);
}

template<class T>
comp_table<T> *add_comp_tbl(comp_db *cdb, sizet initial_capacity = 64, sizet mem_alignment = DEFAULT_MIN_ALIGNMENT)
{
    if ((T::type_id + 1) > cdb->comp_tables.size) {
        arr_resize(&cdb->comp_tables, T::type_id + 1);
    }
    if (!cdb->comp_tables[T::type_id]) {
        auto ctbl = mem_alloc<comp_table<T>>(cdb->comp_tables.arena);
        memset(ctbl, 0, sizeof(comp_table<T>));
        init_comp_tbl(ctbl, cdb->comp_tables.arena, initial_capacity, mem_alignment);
        cdb->comp_tables[T::type_id] = ctbl;
    }
    return (comp_table<T> *)cdb->comp_tables[T::type_id];
}

template<class T>
comp_table<T> *get_comp_tbl(comp_db *cdb)
{
    if (T::type_id < cdb->comp_tables.size) {
        return (comp_table<T> *)cdb->comp_tables[T::type_id];
    }
    return nullptr;
}

template<class T>
bool remove_comp_tbl(comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T>(cdb);
    if (ctbl) {
        terminate_comp_tbl(ctbl);
        mem_free(ctbl, cdb->comp_tables.arena);
        cdb->comp_tables[T::type_id] = {};
        return true;
    }
    return false;
}

void init_comp_db(comp_db *cdb, mem_arena *arena);
void terminate_comp_db(comp_db *cdb);

template<class T>
T *add_comp(u32 ent_id, comp_table<T> *ctbl, const T &copy = {})
{
    T* ret{};
    sizet cid = ctbl->entries.size;
    auto item = hmap_insert(&ctbl->entc_hm, ent_id, cid);
    if (item) {
        arr_push_back(&ctbl->entries, copy);
        ctbl->entries[cid].ent_id = ent_id;
        ret = &ctbl->entries[cid];
    }
    return ret;
}

template<class T>
T *add_comp(u32 ent_id, comp_db *cdb, const T &copy = {})
{
    auto ctbl = get_comp_tbl<T>(cdb);
    return add_comp<T>(ent_id, ctbl, copy);
}

template<class T>
T *add_comp(entity *ent, const T &copy = {})
{
    return add_comp<T>(ent->id, ent->cdb, copy);
}

template<class T>
T *get_comp(u32 ent_id, comp_table<T> *ctbl)
{
    auto fiter = hmap_find(&ctbl->entc_hm, ent_id);
    if (!fiter) {
        return nullptr;
    }
    return &ctbl->entries[fiter->val];
}

template<class T>
T *get_comp(u32 ent_id, comp_db *cdb)
{
    auto ctbl = get_comp_tbl<T>(cdb);
    return get_comp<T>(ent_id, ctbl);
}

template<class T>
T *get_comp(entity *ent)
{
    return get_comp<T>(ent->id, ent->cdb);
}

sizet add_entities(sizet count, sim_region *reg);
entity *add_entity(const entity &copy, sim_region *reg);
entity *add_entity(const char *name, sim_region *reg);
entity *get_entity(u32 ent_id, sim_region *reg);
bool remove_entity(u32 ent_id, sim_region *reg);
bool remove_entity(entity *ent, sim_region *reg);

void init_sim_region(sim_region *reg, mem_arena *arena);
void terminate_sim_region(sim_region *reg);

} // namespace nslib

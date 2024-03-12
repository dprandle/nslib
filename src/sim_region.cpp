#include "sim_region.h"

namespace nslib
{

void comp_db_init(comp_db *cdb, mem_arena *arena)
{
    arr_init(&cdb->comp_tables, arena);
}

void comp_db_terminate(comp_db *cdb)
{
    arr_terminate(&cdb->comp_tables);
}

sizet add_entities(sizet count, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_resize(&reg->ents, ind + count);
    for (sizet i = 0; i < count; ++i) {
        reg->ents[ind + i].id = ++reg->last_id;
        reg->ents[ind + i].cdb = &reg->cdb;
    }
    return ind;
}

entity *add_entity(const entity &copy, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_emplace_back(&reg->ents, copy);
    reg->ents[ind].id = ++reg->last_id;
    reg->ents[ind].cdb = &reg->cdb;
    auto iter = hashmap_set(&reg->entmap, reg->ents[ind].id, ind);
    assert(!iter);
    return &reg->ents[ind];
}

entity *add_entity(const char *name, sim_region *reg)
{
    sizet ind = reg->ents.size;
    arr_emplace_back(&reg->ents, ++reg->last_id, name, &reg->cdb);
    auto iter = hashmap_set(&reg->entmap, reg->ents[ind].id, ind);
    assert(!iter);
    return &reg->ents[ind];
}

entity *get_entity(u32 ent_id, sim_region *reg)
{
    auto fiter = hashmap_find(&reg->entmap, ent_id);
    if (fiter) {
        return &reg->ents[fiter->value];
    }
    return nullptr;
}

bool remove_entity(u32 ent_id, sim_region *reg)
{
    auto fiter = hashmap_find(&reg->entmap, ent_id);
    if (fiter) {
    }
    return false;
}

bool remove_entity(entity *ent, sim_region *reg)
{
    auto rem = hashmap_remove(&reg->entmap, ent->id);
    if (!rem) {
        return false;
    }

    sizet ind = arr_index_of(&reg->ents, ent);
    if (ind != NPOS) {
        if (arr_swap_remove(&reg->ents, ind)) {
            if (ind < reg->ents.size) {
                hashmap_set(&reg->entmap, reg->ents[ind].id, ind);
            }
            return true;
        }
    }
    return false;
}

void sim_region_init(sim_region *reg, mem_arena *arena)
{
    arr_init(&reg->ents, arena);
    comp_db_init(&reg->cdb, arena);
    hashmap_init(&reg->entmap);

    add_comp_tbl<static_model>(&reg->cdb);
    add_comp_tbl<camera>(&reg->cdb);
    add_comp_tbl<transform>(&reg->cdb);
}

void sim_region_terminate(sim_region *reg)
{
    remove_comp_tbl<transform>(&reg->cdb);
    remove_comp_tbl<camera>(&reg->cdb);
    remove_comp_tbl<static_model>(&reg->cdb);

    hashmap_terminate(&reg->entmap);
    comp_db_terminate(&reg->cdb);
    arr_terminate(&reg->ents);
}
} // namespace nslib

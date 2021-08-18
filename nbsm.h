#ifndef NBSM
#define NBSM

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#pragma region "Types"

#pragma region "Hash table"

#define NBSM_HTABLE_DEFAULT_INITIAL_CAPACITY 200
#define NBSM_HTABLE_LOAD_FACTOR_THRESHOLD 0.75

typedef struct
{
    const char *key;
    void *item;
    unsigned int slot;
} HTableEntry;

typedef struct
{
    HTableEntry **internal_array;
    unsigned int capacity;
    unsigned int count;
    float load_factor;
} HTable;

typedef void (*HTableDestroyItemFunc)(void *);

#pragma endregion // Hash table

#pragma region "State machine"

#define NBSM_MAX_TRANSITIONS_PER_STATE 8
#define NBSM_MAX_CONDITIONS_PER_TRANSITION 16

typedef enum
{
    NBSM_INTEGER,
    NBSM_FLOAT,
    NBSM_BOOLEAN
} NBSM_ValueType;

typedef enum
{
    NBSM_EQUALS,
    NBSM_LT,
    NBSM_LTE,
    NBSM_GT,
    NBSM_GTE
} NBSM_ConditionType;

typedef struct
{
    NBSM_ValueType type;

    union
    {
        int i;
        float f;
        bool b;
    } value;
} NBSM_Value;

typedef struct __NBSM_State NBSM_State;

typedef struct
{
    HTable *states;
    HTable *values;
    NBSM_State *current;
} NBSM_Machine;

typedef bool (*NBSM_ConditionFunc)(NBSM_Value *v1, NBSM_Value *v2);

typedef struct
{
    NBSM_ConditionFunc func;
    NBSM_Value *v1;
    NBSM_Value v2;
} NBSM_Condition;

typedef struct
{
    NBSM_State *target_state;
    NBSM_Condition conditions[NBSM_MAX_CONDITIONS_PER_TRANSITION];
    unsigned int condition_count;
} NBSM_Transition;

struct __NBSM_State
{
    const char *name;
    NBSM_Transition transitions[NBSM_MAX_TRANSITIONS_PER_STATE];
    unsigned int transition_count;
};

#pragma endregion // State machine

#pragma region "Descriptors"

typedef struct
{
    const char *var_name;
    NBSM_ConditionType type;
    NBSM_Value value; 
} NBSM_ConditionDesc;

typedef struct __NBSM_StateDesc NBSM_StateDesc;
typedef struct __NBSM_TransitionDesc NBSM_TransitionDesc;

struct __NBSM_TransitionDesc
{
    NBSM_StateDesc *src_state;
    NBSM_StateDesc *target_state;
    NBSM_ConditionDesc conditions[NBSM_MAX_CONDITIONS_PER_TRANSITION];
    unsigned int condition_count;
    void *user_data; // user defined data (used by the editor)
    NBSM_TransitionDesc *next;
    NBSM_TransitionDesc *prev;
};

struct __NBSM_StateDesc
{
    char *name;
    NBSM_TransitionDesc *transitions;
    unsigned int transition_count;
    bool is_initial;
    void *user_data; // user defined data (used by the editor)
    NBSM_StateDesc *next;
    NBSM_StateDesc *prev;
};

typedef struct __NBSM_ValueDesc NBSM_ValueDesc;

struct __NBSM_ValueDesc
{
    char *name;
    NBSM_ValueType type;
    void *user_data; // user defined data (used by the editor)
    NBSM_ValueDesc *next;
    NBSM_ValueDesc *prev;
};

typedef struct
{
    NBSM_StateDesc *states;
    unsigned int state_count;
    NBSM_ValueDesc *values;
    unsigned int value_count;
} NBSM_MachineDesc;

#pragma endregion // Descriptors

#pragma endregion // Types

#pragma region "Public API"

NBSM_Machine *NBSM_Create(NBSM_MachineDesc *desc);
void NBSM_Destroy(NBSM_Machine *machine);
void NBSM_Update(NBSM_Machine *machine);
void NBSM_SetInteger(NBSM_Machine *machine, const char *var_name, int value);
void NBSM_SetFloat(NBSM_Machine *machine, const char *var_name, float value);
void NBSM_SetBoolean(NBSM_Machine *machine, const char *var_name, bool value);

#pragma endregion // Public API

#pragma region "Descritor public API"

#ifdef NBSM_DESCRIPTOR_API

void NBSM_InitMachineDesc(NBSM_MachineDesc *machine_desc);
void NBSM_DeinitMachineDesc(NBSM_MachineDesc *machine_desc);
NBSM_StateDesc *NBSM_AddStateDesc(NBSM_MachineDesc *machine_desc, char *name, void *user_data);
NBSM_StateDesc *NBSM_RemoveStateDesc(NBSM_MachineDesc *machine_desc, NBSM_StateDesc *desc);
NBSM_TransitionDesc *NBSM_AddTransitionDesc(NBSM_StateDesc *from, NBSM_StateDesc *to, void *user_data);
NBSM_TransitionDesc *NBSM_RemoveTransitionDesc(NBSM_TransitionDesc *desc);
NBSM_ValueDesc *NBSM_AddValueDesc(NBSM_MachineDesc *machine_desc, char *name, NBSM_ValueType type, void *user_name);
NBSM_ValueDesc *NBSM_RemoveValueDesc(NBSM_MachineDesc *machine_desc, NBSM_ValueDesc *desc);

#endif // NBSM_DESCRIPTOR_API

#pragma endregion // Descriptor public API

#pragma region "Implementation"

#ifdef NBSM_IMPL

#pragma region "Hash table"

static unsigned long HashSDBM(const char *str)
{
    unsigned long hash = 0;
    int c;

    while ((c = *str++))
    {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static HTableEntry *FindHTableEntry(HTable *htable, const char *key)
{
    unsigned long hash = HashSDBM(key);
    unsigned int slot;

    //quadratic probing

    HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        if (current_entry != NULL && strcmp(current_entry->key, key) == 0)
        {
            return current_entry;
        }

        i++;
    } while (i < htable->capacity);
    
    return NULL;
}

static unsigned int FindFreeSlotInHTable(HTable *htable, HTableEntry *entry, bool *use_existing_slot)
{
    unsigned long hash = HashSDBM(entry->key);
    unsigned int slot;

    // quadratic probing

    HTableEntry *current_entry;
    unsigned int i = 0;

    do
    {
        slot = (hash + (int)pow(i, 2)) % htable->capacity;
        current_entry = htable->internal_array[slot];

        i++;
    } while (current_entry != NULL && current_entry->key != entry->key);

    if (current_entry != NULL) // it means the current entry as the same key as the inserted entry
    {
        *use_existing_slot = true;

        NBSM_Dealloc(current_entry);
    }
    
    return slot;
}

static HTable *CreateHTableWithCapacity(unsigned int capacity)
{
    HTable *htable = NBSM_Alloc(sizeof(HTable));

    htable->internal_array = NBSM_Alloc(sizeof(HTableEntry *) * capacity);
    htable->capacity = capacity;
    htable->count = 0;
    htable->load_factor = 0;

    for (unsigned int i = 0; i < htable->capacity; i++)
        htable->internal_array[i] = NULL;

    return htable;
}

static void InsertHTableEntry(HTable *htable, HTableEntry *entry)
{
    bool use_existing_slot = false;
    unsigned int slot = FindFreeSlotInHTable(htable, entry, &use_existing_slot);

    entry->slot = slot;
    htable->internal_array[slot] = entry;

    if (!use_existing_slot)
    {
        htable->count++;
        htable->load_factor = (float)htable->count / htable->capacity;
    }
}

static void RemoveHTableEntry(HTable *htable, HTableEntry *entry)
{
    htable->internal_array[entry->slot] = NULL;

    NBSM_Dealloc(entry);

    htable->count--;
    htable->load_factor = htable->count / htable->capacity;
}

static HTable *CreateHTable()
{
    return CreateHTableWithCapacity(NBSM_HTABLE_DEFAULT_INITIAL_CAPACITY);
}

static void DestroyHTable(
        HTable *htable, bool destroy_items, HTableDestroyItemFunc destroy_item_func, bool destroy_keys)
{
    for (unsigned int i = 0; i < htable->capacity; i++)
    {
        HTableEntry *entry = htable->internal_array[i];

        if (entry)
        {
            if (destroy_items)
                destroy_item_func(entry->item);

            if (destroy_keys)
                NBSM_Dealloc((void*)entry->key);

            NBSM_Dealloc(entry);
        }
    }

    NBSM_Dealloc(htable->internal_array);
    NBSM_Dealloc(htable);
}

static void GrowHTable(HTable *htable)
{
    unsigned int old_capacity = htable->capacity;
    unsigned int new_capacity = old_capacity * 2;
    HTableEntry** old_internal_array = htable->internal_array;
    HTableEntry** new_internal_array = NBSM_Alloc(sizeof(HTableEntry*) * new_capacity);

    for (unsigned int i = 0; i < new_capacity; i++)
    {
        new_internal_array[i] = NULL;
    }

    htable->internal_array = new_internal_array;
    htable->capacity = new_capacity;
    htable->count = 0;
    htable->load_factor = 0;

    // rehash

    for (unsigned int i = 0; i < old_capacity; i++)
    {
        if (old_internal_array[i])
            InsertHTableEntry(htable, old_internal_array[i]);
    }

    NBSM_Dealloc(old_internal_array);
}

static void AddToHTable(HTable *htable, const char *key, void *item)
{
    HTableEntry *entry = NBSM_Alloc(sizeof(HTableEntry));

    entry->key = key;
    entry->item = item;

    InsertHTableEntry(htable, entry);

    if (htable->load_factor >= NBSM_HTABLE_LOAD_FACTOR_THRESHOLD)
        GrowHTable(htable);
}

static void *GetInHTable(HTable *htable, const char *key)
{
    HTableEntry *entry = FindHTableEntry(htable, key);

    return entry ? entry->item : NULL;
}

static void *RemoveFromHTable(HTable *htable, const char *key)
{
    HTableEntry *entry = FindHTableEntry(htable, key);

    if (entry)
    {
        void *item = entry->item;

        RemoveHTableEntry(htable, entry);

        return item;
    }

    return NULL;
}

#pragma endregion // Hash table

#pragma region "State machine"

static void BuildState(NBSM_Machine *machine, NBSM_State *state, NBSM_StateDesc *desc);
static void BuildTransition(NBSM_Machine *machine, NBSM_Transition *transition, NBSM_TransitionDesc *desc);
static void BuildCondition(NBSM_Machine *machine, NBSM_Condition *condition, NBSM_ConditionDesc *desc);
static bool ConditionEquals(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionLT(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionLTE(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionGT(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionGTE(NBSM_Value *v1, NBSM_Value *v2);
static void DestroyMachineValue(void *v);
static void DestroyMachineState(void *v);

NBSM_Machine *NBSM_Create(NBSM_MachineDesc *desc)
{
    NBSM_Machine *machine = NBSM_Alloc(sizeof(NBSM_Machine));

    machine->states = CreateHTableWithCapacity(desc->state_count);
    machine->values = CreateHTable();
    machine->current = NULL;

    for (unsigned int i = 0; i < desc->value_count; i++)
    {
        NBSM_Value *value = NBSM_Alloc(sizeof(NBSM_Value));

        memset(value, 0, sizeof(NBSM_Value));

        AddToHTable(machine->values, desc->values[i].name, value);
    }

    for (unsigned int i = 0; i < desc->state_count; i++)
    {
        NBSM_StateDesc *state_desc = &desc->states[i];
        NBSM_State *state = NBSM_Alloc(sizeof(NBSM_State));
        
        BuildState(machine, state, state_desc);
        AddToHTable(machine->states, state->name, state);

        if (state_desc->is_initial)
            machine->current = state;
    }

    assert(machine->current);

    return machine;
}

void NBSM_Destroy(NBSM_Machine *machine)
{
    DestroyHTable(machine->values, true, DestroyMachineValue, false);
    DestroyHTable(machine->states, true, DestroyMachineState, false);
}

void NBSM_Update(NBSM_Machine *machine)
{
    for (unsigned int i = 0; i < machine->current->transition_count; i++)
    {
        NBSM_Transition *t = &machine->current->transitions[i];

        assert(t->condition_count > 0);

        bool res = true;

        for (unsigned int j = 0; j < t->condition_count; j++)
        {
            NBSM_Condition *c = &t->conditions[j];

            if (!c->func(c->v1, &c->v2))
            {
                res = false;

                break;
            }
        }

        if (res)
        {
            machine->current = t->target_state;

            break;
        }
    }
}

void NBSM_SetInteger(NBSM_Machine *machine, const char *var_name, int value)
{
    NBSM_Value *v = GetInHTable(machine->values, var_name);

    assert(v->type == NBSM_INTEGER);

    v->value.i = value;
}

void NBSM_SetFloat(NBSM_Machine *machine, const char *var_name, float value)
{
    NBSM_Value *v = GetInHTable(machine->values, var_name);

    assert(v->type == NBSM_FLOAT);

    v->value.f = value;
}

void NBSM_SetBoolean(NBSM_Machine *machine, const char *var_name, bool value)
{
    NBSM_Value *v = GetInHTable(machine->values, var_name);

    assert(v->type == NBSM_BOOLEAN);

    v->value.b = value;
}

static void BuildState(NBSM_Machine *machine, NBSM_State *state, NBSM_StateDesc *desc)
{
    state->name = desc->name;
    state->transition_count = desc->transition_count;

    for (unsigned int i = 0; i < state->transition_count; i++)
        BuildTransition(machine, &state->transitions[i], &desc->transitions[i]);
}

static void BuildTransition(NBSM_Machine *machine, NBSM_Transition *transition, NBSM_TransitionDesc *desc)
{
    transition->condition_count = desc->condition_count;

    for (unsigned int i = 0; i < transition->condition_count; i++)
        BuildCondition(machine, &transition->conditions[i], &desc->conditions[i]);
}

static void BuildCondition(NBSM_Machine *machine, NBSM_Condition *condition, NBSM_ConditionDesc *desc)
{
    static NBSM_ConditionFunc condition_funcs[] = {
        ConditionEquals,
        ConditionLT,
        ConditionLTE,
        ConditionGT,
        ConditionGTE
    };

    assert(condition->v1->type == condition->v2.type);

    condition->func = condition_funcs[desc->type];
    condition->v1 = GetInHTable(machine->values, desc->var_name);
    condition->v2 = desc->value;
}

static bool ConditionEquals(NBSM_Value *v1, NBSM_Value *v2)
{
    if (v1->type == NBSM_INTEGER && v2->type == NBSM_INTEGER)
        return v1->value.i == v2->value.i;

    if (v1->type == NBSM_FLOAT && v2->type == NBSM_FLOAT)
        return v1->value.f == v2->value.f;

    if (v1->type == NBSM_BOOLEAN && v2->type == NBSM_BOOLEAN)
        return v1->value.b == v2->value.b;

    return false;
}

static bool ConditionLT(NBSM_Value *v1, NBSM_Value *v2)
{
    assert((v1->type == NBSM_INTEGER || v1->type == NBSM_FLOAT) && v2->type == NBSM_INTEGER || v2->type == NBSM_FLOAT);

    if (v1->type == NBSM_INTEGER && v2->type == NBSM_INTEGER)
        return v1->value.i < v2->value.i;

    if (v1->type == NBSM_FLOAT && v2->type == NBSM_FLOAT)
        return v1->value.f < v2->value.f;

    return false;
}

static bool ConditionLTE(NBSM_Value *v1, NBSM_Value *v2)
{
    assert((v1->type == NBSM_INTEGER || v1->type == NBSM_FLOAT) && v2->type == NBSM_INTEGER || v2->type == NBSM_FLOAT);

    if (v1->type == NBSM_INTEGER && v2->type == NBSM_INTEGER)
        return v1->value.i <= v2->value.i;

    if (v1->type == NBSM_FLOAT && v2->type == NBSM_FLOAT)
        return v1->value.f <= v2->value.f;

    return false;
}

static bool ConditionGT(NBSM_Value *v1, NBSM_Value *v2)
{
    assert((v1->type == NBSM_INTEGER || v1->type == NBSM_FLOAT) && v2->type == NBSM_INTEGER || v2->type == NBSM_FLOAT);

    if (v1->type == NBSM_INTEGER && v2->type == NBSM_INTEGER)
        return v1->value.i > v2->value.i;

    if (v1->type == NBSM_FLOAT && v2->type == NBSM_FLOAT)
        return v1->value.f > v2->value.f;

    return false;
}

static bool ConditionGTE(NBSM_Value *v1, NBSM_Value *v2)
{
    assert((v1->type == NBSM_INTEGER || v1->type == NBSM_FLOAT) && v2->type == NBSM_INTEGER || v2->type == NBSM_FLOAT);

    if (v1->type == NBSM_INTEGER && v2->type == NBSM_INTEGER)
        return v1->value.i >= v2->value.i;

    if (v1->type == NBSM_FLOAT && v2->type == NBSM_FLOAT)
        return v1->value.f >= v2->value.f;

    return false;
}

static void DestroyMachineValue(void *v)
{
    NBSM_Dealloc(v);
}

static void DestroyMachineState(void *v)
{
    NBSM_Dealloc(v);
}

#pragma endregion // State machine

#pragma region "Descritors"

#ifdef NBSM_DESCRIPTOR_API

void NBSM_InitMachineDesc(NBSM_MachineDesc *machine_desc)
{
    machine_desc->states = NULL;
    machine_desc->state_count = 0;
    machine_desc->values = NULL;
    machine_desc->value_count = 0;
}

void NBSM_DeinitMachineDesc(NBSM_MachineDesc *machine_desc)
{
    // TODO
}

NBSM_StateDesc *NBSM_AddStateDesc(NBSM_MachineDesc *machine_desc, char *name, void *user_data)
{
    NBSM_StateDesc *desc = NBSM_Alloc(sizeof(NBSM_StateDesc));

    desc->name = name;
    desc->is_initial = false;
    desc->user_data = user_data;
    desc->transitions = NULL;
    desc->transition_count = 0;

    if (!machine_desc->states)
    {
        desc->prev = NULL;
        desc->next = NULL;

        machine_desc->states = desc;
    }
    else
    {
        NBSM_StateDesc *s = machine_desc->states;

        while (s->next)
            s = s->next;

        assert(!s->next);

        desc->prev = s;
        desc->next = NULL;

        s->next = desc;
    }

    machine_desc->state_count++;

    return desc;
}

NBSM_StateDesc *NBSM_RemoveStateDesc(NBSM_MachineDesc *machine_desc, NBSM_StateDesc *desc)
{
    if (!desc->prev)
    {
        // first item of the list
        machine_desc->states = desc->next;

        if (desc->next)
            desc->next->prev = NULL;
    }
    else
    {
        desc->prev->next = desc->next;
        
        if (desc->next)
            desc->next->prev = desc->prev;
    }

    machine_desc->state_count--;

    return desc;
}

NBSM_TransitionDesc *NBSM_AddTransitionDesc(NBSM_StateDesc *from, NBSM_StateDesc *to, void *user_data)
{
    NBSM_TransitionDesc *desc = NBSM_Alloc(sizeof(NBSM_TransitionDesc));

    desc->src_state = from;
    desc->target_state = to;
    desc->user_data = user_data;
    desc->condition_count = 0;

    if (!from->transitions)
    {
        desc->prev = NULL;
        desc->next = NULL;

        from->transitions = desc;
    }
    else
    {
        NBSM_TransitionDesc *t = from->transitions;

        while (t->next)
            t = t->next;

        assert(!t->next);

        desc->prev = t;
        desc->next = NULL;

        t->next = desc;
    }

    from->transition_count++;

    return desc;
}

NBSM_TransitionDesc *NBSM_RemoveTransitionDesc(NBSM_TransitionDesc *desc)
{
    NBSM_StateDesc *from = desc->src_state;

    if (!desc->prev)
    {
        // first item of the list
        from->transitions = desc->next;

        if (desc->next)
            desc->next->prev = NULL;
    }
    else
    {
        desc->prev->next = desc->next;
        
        if (desc->next)
            desc->next->prev = desc->prev;
    }

    from->transition_count--;

    return desc;
}

NBSM_ValueDesc *NBSM_AddValueDesc(NBSM_MachineDesc *machine_desc, char *name, NBSM_ValueType type, void *user_data)
{
    NBSM_ValueDesc *desc = NBSM_Alloc(sizeof(NBSM_ValueDesc));

    desc->name = name;
    desc->type = type;
    desc->user_data = user_data;

    if (!machine_desc->values)
    {
        desc->prev = NULL;
        desc->next = NULL;

        machine_desc->values = desc;
    }
    else
    {
        NBSM_ValueDesc *v = machine_desc->values;

        while (v->next)
            v = v->next;

        assert(!v->next);

        desc->prev = v;
        desc->next = NULL;

        v->next = desc;
    }

    machine_desc->value_count++;

    return desc;
}

NBSM_ValueDesc *NBSM_RemoveValueDesc(NBSM_MachineDesc *machine_desc, NBSM_ValueDesc *desc)
{
    if (!desc->prev)
    {
        // first item of the list
        machine_desc->values = desc->next;

        if (desc->next)
            desc->next->prev = NULL;
    }
    else
    {
        desc->prev->next = desc->next;
        
        if (desc->next)
            desc->next->prev = desc->prev;
    }

    machine_desc->value_count--;

    return desc;
}

#endif // NBSM_DESCRIPTOR_API

#pragma endregion // Descriptors

#endif // NBSM_IMPL

#pragma endregion // Implementation

#endif // NBSM
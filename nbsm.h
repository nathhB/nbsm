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
    NBSM_EQ,
    NBSM_NEQ,
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
    HTable *variables;
    NBSM_State *current;
} NBSM_Machine;

typedef bool (*NBSM_ConditionFunc)(NBSM_Value v1, NBSM_Value v2);

typedef struct
{
    NBSM_ConditionFunc func;
    NBSM_Value *v1; // points to a variable
    NBSM_Value v2; // constant value
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

#pragma endregion // Types

#pragma region "Public API"

NBSM_Machine *NBSM_Create(void);
void NBSM_Destroy(NBSM_Machine *machine);
void NBSM_Update(NBSM_Machine *machine);
void NBSM_SetInteger(NBSM_Machine *machine, const char *var_name, int value);
void NBSM_SetFloat(NBSM_Machine *machine, const char *var_name, float value);
void NBSM_SetBoolean(NBSM_Machine *machine, const char *var_name, bool value);

#pragma endregion // Public API

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

static void DestroyMachineValue(void *v);
static void DestroyMachineState(void *v);

NBSM_Machine *NBSM_Create(void)
{
    NBSM_Machine *machine = NBSM_Alloc(sizeof(NBSM_Machine));

    machine->states = CreateHTable();
    machine->variables = CreateHTable();
    machine->current = NULL;

    return machine;
}

void NBSM_Destroy(NBSM_Machine *machine)
{
    DestroyHTable(machine->variables, true, DestroyMachineValue, false);
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

            if (!c->func(*c->v1, c->v2))
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
    NBSM_Value *v = GetInHTable(machine->variables, var_name);

    assert(v->type == NBSM_INTEGER);

    v->value.i = value;
}

void NBSM_SetFloat(NBSM_Machine *machine, const char *var_name, float value)
{
    NBSM_Value *v = GetInHTable(machine->variables, var_name);

    assert(v->type == NBSM_FLOAT);

    v->value.f = value;
}

void NBSM_SetBoolean(NBSM_Machine *machine, const char *var_name, bool value)
{
    NBSM_Value *v = GetInHTable(machine->variables, var_name);

    assert(v->type == NBSM_BOOLEAN);

    v->value.b = value;
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

#endif // NBSM_IMPL

#pragma endregion // Implementation

#endif // NBSM
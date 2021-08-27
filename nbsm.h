#ifndef NBSM
#define NBSM

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifdef NBSM_JSON_BUILDER

#include "json.h" // https://github.com/sheredom/json.h

#endif // NBSM_JSON_BUILDER

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

#ifndef NBSM_Alloc
#define NBSM_Alloc malloc
#endif

#ifndef NBSM_Dealloc
#define NBSM_Dealloc free
#endif

#define NBSM_Assert(cond) \
    {                     \
        if (!(cond))      \
            abort();      \
    }

#define NBSM_CONST_I(v) ((NBSM_ConditionOperand){ NBSM_OPERAND_CONST, .data = { .constant = (NBSM_Value){ NBSM_INTEGER, { .i = v } } } })
#define NBSM_CONST_F(v) ((NBSM_ConditionOperand){ NBSM_OPERAND_CONST, .data = { .constant = (NBSM_Value){ NBSM_FLOAT, { .f = v } } } })
#define NBSM_TRUE ((NBSM_ConditionOperand){ NBSM_OPERAND_CONST, .data = { .constant = (NBSM_Value){ NBSM_BOOLEAN, { .b = true } } } })
#define NBSM_FALSE ((NBSM_ConditionOperand){ NBSM_OPERAND_CONST, .data = { .constant = (NBSM_Value){ NBSM_BOOLEAN, { .b = false } } } })
#define NBSM_VAR(machine, name) ((NBSM_ConditionOperand){ NBSM_OPERAND_VAR, .data = { .var = NBSM_GetVariable(machine, name) } })

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

typedef enum
{
    NBSM_OPERAND_CONST,
    NBSM_OPERAND_VAR
} NBSM_ConditionOperandType;

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
    void *user_data;
} NBSM_Machine;

typedef bool (*NBSM_ConditionFunc)(NBSM_Value *v1, NBSM_Value *v2);

typedef struct __NBSM_Condition NBSM_Condition;

typedef struct
{
    NBSM_ConditionOperandType type;

    union
    {
        NBSM_Value constant;
        NBSM_Value *var;
    } data;
} NBSM_ConditionOperand;

struct __NBSM_Condition
{
    NBSM_ConditionFunc func;
    NBSM_Value *left_op; // left operand, points to a state machine's variable

    // right operand, can be either a constant value or a state machine's variable
    NBSM_ConditionOperand right_op; 

    NBSM_Condition *next;
};

typedef struct __NBSM_Transition NBSM_Transition;

struct __NBSM_Transition
{
    NBSM_State *target_state;
    NBSM_Condition *conditions;
    NBSM_Transition *next;
};

typedef void (*NBSM_StateHookFunc)(NBSM_Machine *machine);

struct __NBSM_State
{
    const char *name;
    NBSM_Transition *transitions;
    NBSM_StateHookFunc on_enter;
    NBSM_StateHookFunc on_exit;
    NBSM_StateHookFunc on_update;
    void *user_data;
};

typedef struct
{
    const char *name;
    bool is_initial;
} NBSM_StateBlueprint;

typedef struct
{
    const char *name;
    NBSM_ValueType type;
} NBSM_VariableBlueprint;

typedef struct
{
    NBSM_ConditionOperandType type;

    union
    {
        NBSM_Value constant;
        const char *var_name;
    } data;
} NBSM_ConditionOperandBlueprint;

typedef struct
{
    unsigned int transition_idx;
    NBSM_ConditionType type;
    const char *var_name;
    NBSM_ConditionOperandBlueprint right_op;
} NBSM_ConditionBlueprint;

typedef struct
{
    const char *from;
    const char *to;
    NBSM_ConditionBlueprint *conditions;
    unsigned int condition_count;
} NBSM_TransitionBlueprint;

typedef struct
{
    NBSM_StateBlueprint *states;
    unsigned int state_count;

    NBSM_VariableBlueprint *variables;
    unsigned int variable_count;

    NBSM_TransitionBlueprint *transitions;
    unsigned int transition_count; 
} NBSM_MachineBuilder;

#pragma endregion // State machine

#pragma endregion // Types

#pragma region "Public API"

// Create a new empty state machine
NBSM_Machine *NBSM_Create(void);

// Create a new state machine from a machine builder
NBSM_Machine *NBSM_Build(NBSM_MachineBuilder *builder);

// Destroy the state machine and release memory
void NBSM_Destroy(NBSM_Machine *machine);

// Update the state machine (check if any transition needs to be executed based on conditions)
void NBSM_Update(NBSM_Machine *machine);

// Change the current state of the state machine, ignoring transitions and conditions
void NBSM_ChangeState(NBSM_Machine *machine, const char *name);

// Add a state to the state machine
void NBSM_AddState(NBSM_Machine *machine, const char *name, bool is_initial);

// Attach some user defined data to a given state
void NBSM_AttachDataToState(NBSM_Machine *machine, const char *name, void *user_data);

// Add an "OnEnter" hook on the given state
void NBSM_OnStateEnter(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func);

// Add an "OnExit" hook on the given state
void NBSM_OnStateExit(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func);

// Add an "OnUpdate" hook on the given state
void NBSM_OnStateUpdate(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func);

// Add a new transition between two states
NBSM_Transition *NBSM_AddTransition(NBSM_Machine *machine, const char *from, const char *to);

// Add a condition to a transition
void NBSM_AddCondition(
    NBSM_Machine *machine, NBSM_Transition *transition, const char *var_name, NBSM_ConditionType type, NBSM_ConditionOperand right_op);

// Add a new variable to the state machine
NBSM_Value *NBSM_AddVariable(NBSM_Machine *machine, const char *name, NBSM_ValueType type);

// Add a new integer variable to the state machine
NBSM_Value *NBSM_AddInteger(NBSM_Machine *machine, const char *name);

// Add a new float variable to the state machine
NBSM_Value *NBSM_AddFloat(NBSM_Machine *machine, const char *name);

// Add a new boolean variable to the state machine
NBSM_Value *NBSM_AddBoolean(NBSM_Machine *machine, const char *name);

// Set the value of an integer variable of the state machine
void NBSM_SetInteger(NBSM_Value *var, int value);

// Set the value of a float variable of the state machine
void NBSM_SetFloat(NBSM_Value *var, float value);

// Set the value of a boolean variable of the state machine
void NBSM_SetBoolean(NBSM_Value *var, bool value);

// Get the value of an integer variable of the state machine
int NBSM_GetInteger(NBSM_Value *var);

// Get the value of a float variable of the state machine
float NBSM_GetFloat(NBSM_Value *var);

// Get the value of a boolean variable of the state machine
bool NBSM_GetBoolean(NBSM_Value *var);

// Get a variable from the state machine
NBSM_Value *NBSM_GetVariable(NBSM_Machine *machine, const char *name);

#ifdef NBSM_JSON_BUILDER

// Create a new machine builder from a JSON file
NBSM_MachineBuilder *NBSM_CreateBuilderFromJSON(const char *json);

#endif // NBSM_JSON_BUILDER

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

static bool DoesEntryExist(HTable *htable, const char *key)
{
    return !!FindHTableEntry(htable, key);
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
                NBSM_Dealloc((void *)entry->key);

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
    HTableEntry **old_internal_array = htable->internal_array;
    HTableEntry **new_internal_array = NBSM_Alloc(sizeof(HTableEntry *) * new_capacity);

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

#pragma region "Public API"

static void ChangeState(NBSM_Machine *machine, NBSM_State *state);
static NBSM_ConditionFunc GetConditionFunction(NBSM_ConditionType type);
static bool ConditionEQ(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionNEQ(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionLT(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionLTE(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionGT(NBSM_Value *v1, NBSM_Value *v2);
static bool ConditionGTE(NBSM_Value *v1, NBSM_Value *v2);
static void DestroyMachineValue(void *ptr);
static void DestroyMachineState(void *ptr);
static void DestroyMachineTransition(NBSM_Transition *transition);

#ifdef NBSM_JSON_BUILDER

static void LoadVariablesFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *var_arr);
static void LoadStatesFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *state_arr);
static void LoadTransitionsFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *trans_arr);
static void LoadConditionsFromJSON(NBSM_TransitionBlueprint *transition, unsigned int trans_idx, struct json_array_s *cond_arr);
static NBSM_ConditionOperandBlueprint LoadConditionOperandFromJSON(struct json_object_s *op_obj);
static NBSM_ValueType GetVariableTypeFromJSON(const char *type_str);
static NBSM_ConditionType GetConditionTypeFromJSON(const char *type_str);

#endif // NBSM_JSON_BUILDER

NBSM_Machine *NBSM_Create(void)
{
    NBSM_Machine *machine = NBSM_Alloc(sizeof(NBSM_Machine));

    machine->states = CreateHTable();
    machine->variables = CreateHTable();
    machine->current = NULL;
    machine->user_data = NULL;

    return machine;
}

NBSM_Machine *NBSM_Build(NBSM_MachineBuilder *builder)
{
    NBSM_Machine *machine = NBSM_Create();

    for (unsigned int i = 0; i < builder->state_count; i++)
    {
        NBSM_StateBlueprint *sb = &builder->states[i];

        NBSM_AddState(machine, sb->name, sb->is_initial);
    }

    for (unsigned int i = 0; i < builder->variable_count; i++)
    {
        NBSM_VariableBlueprint *vb = &builder->variables[i];

        NBSM_AddVariable(machine, vb->name, vb->type);
    }

    for (unsigned int i = 0; i < builder->transition_count; i++)
    {
        NBSM_TransitionBlueprint *tb = &builder->transitions[i];
        NBSM_Transition *t = NBSM_AddTransition(machine, tb->from, tb->to);

        for (unsigned int j = 0; j < tb->condition_count; j++)
        {
            NBSM_ConditionBlueprint *cb = &tb->conditions[j];
            NBSM_ConditionOperand right_op = { .type = cb->right_op.type };

            if (cb->right_op.type == NBSM_OPERAND_CONST)
                right_op.data.constant = cb->right_op.data.constant;
            else if (cb->right_op.type == NBSM_OPERAND_VAR)
                right_op.data.var = NBSM_GetVariable(machine, cb->right_op.data.var_name);

            NBSM_AddCondition(machine, t, cb->var_name, cb->type, right_op);
        }
    }

    return machine;
}

void NBSM_Destroy(NBSM_Machine *machine)
{
    DestroyHTable(machine->variables, true, DestroyMachineValue, false);
    DestroyHTable(machine->states, true, DestroyMachineState, false);

    NBSM_Dealloc(machine);
}

void NBSM_Update(NBSM_Machine *machine)
{
    NBSM_Assert(machine->current);

    NBSM_Transition *t = machine->current->transitions;

    while (t)
    {
        if (t->conditions == NULL)
            break;

        bool res = true;

        NBSM_Condition *c = t->conditions;

        while (c)
        {
            NBSM_Value *v2;

            v2 = (c->right_op.type == NBSM_OPERAND_CONST) ? &c->right_op.data.constant : c->right_op.data.var;

            if (!c->func(c->left_op, v2))
            {
                res = false;

                break;
            }

            c = c->next;
        }

        if (res)
            break;

        t = t->next;
    }

    if (t)
        ChangeState(machine, t->target_state);

    if (machine->current->on_update)
        machine->current->on_update(machine);
}

void NBSM_ChangeState(NBSM_Machine *machine, const char *name)
{
    NBSM_State *s = GetInHTable(machine->states, name);

    NBSM_Assert(s);

    ChangeState(machine, s);
}

void NBSM_AddState(NBSM_Machine *machine, const char *name, bool is_initial)
{
    NBSM_Assert(!DoesEntryExist(machine->states, name));

    NBSM_State *s = NBSM_Alloc(sizeof(NBSM_State));

    s->name = name;
    s->transitions = NULL;
    s->user_data = NULL;
    s->on_enter = NULL;
    s->on_exit = NULL;
    s->on_update = NULL;

    AddToHTable(machine->states, name, s);

    if (is_initial)
    {
        NBSM_Assert(!machine->current);

        machine->current = s;
    }
}

void NBSM_AttachDataToState(NBSM_Machine *machine, const char *name, void *user_data)
{
    NBSM_State *s = GetInHTable(machine->states, name);

    NBSM_Assert(s);

    s->user_data = user_data;
}

void NBSM_OnStateEnter(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func)
{
    NBSM_State *s = GetInHTable(machine->states, name);

    NBSM_Assert(s);

    s->on_enter = hook_func;
}

void NBSM_OnStateExit(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func)
{
    NBSM_State *s = GetInHTable(machine->states, name);

    NBSM_Assert(s);

    s->on_exit = hook_func;
}

void NBSM_OnStateUpdate(NBSM_Machine *machine, const char *name, NBSM_StateHookFunc hook_func)
{
    NBSM_State *s = GetInHTable(machine->states, name);

    NBSM_Assert(s);

    s->on_update = hook_func;
}

NBSM_Transition *NBSM_AddTransition(NBSM_Machine *machine, const char *from, const char *to)
{
    NBSM_State *from_s = GetInHTable(machine->states, from);

    NBSM_Assert(from_s);

    NBSM_State *to_s = GetInHTable(machine->states, to);

    NBSM_Assert(to_s);

    NBSM_Transition *new_t = NBSM_Alloc(sizeof(NBSM_Transition));

    new_t->target_state = to_s;
    new_t->conditions = NULL;
    new_t->next = NULL;

    if (!from_s->transitions)
    {
        from_s->transitions = new_t;
    }
    else
    {
        NBSM_Transition *t = from_s->transitions;

        while (t->next)
            t = t->next;

        t->next = new_t;
    }

    return new_t;
}

void NBSM_AddCondition(
    NBSM_Machine *machine, NBSM_Transition *transition, const char *var_name, NBSM_ConditionType type, NBSM_ConditionOperand right_op)
{
    NBSM_Condition *new_c = NBSM_Alloc(sizeof(NBSM_Condition));
    NBSM_Value *var = NBSM_GetVariable(machine, var_name);

    NBSM_Assert(var);
    NBSM_Assert(var->type == (right_op.type == NBSM_OPERAND_CONST ? right_op.data.constant.type : right_op.data.var->type));

    new_c->left_op = var;
    new_c->right_op = right_op;
    new_c->func = GetConditionFunction(type);
    new_c->next = NULL;

    if (!transition->conditions)
    {
        transition->conditions = new_c;
    }
    else
    {
        NBSM_Condition *c = transition->conditions;

        while (c->next)
            c = c->next;

        c->next = new_c;
    }
}

NBSM_Value *NBSM_AddVariable(NBSM_Machine *machine, const char *name, NBSM_ValueType type)
{
    NBSM_Assert(!DoesEntryExist(machine->variables, name));

    NBSM_Value *v = NBSM_Alloc(sizeof(NBSM_Value));

    v->type = type;

    memset(&v->value, 0, sizeof(v->value));

    AddToHTable(machine->variables, name, v);

    return v;
}

NBSM_Value *NBSM_AddInteger(NBSM_Machine *machine, const char *name)
{
    return NBSM_AddVariable(machine, name, NBSM_INTEGER);
}

NBSM_Value *NBSM_AddFloat(NBSM_Machine *machine, const char *name)
{
    return NBSM_AddVariable(machine, name, NBSM_FLOAT);
}

NBSM_Value *NBSM_AddBoolean(NBSM_Machine *machine, const char *name)
{
    return NBSM_AddVariable(machine, name, NBSM_BOOLEAN);
}

void NBSM_SetInteger(NBSM_Value *var, int value)
{
    NBSM_Assert(var->type == NBSM_INTEGER);

    var->value.i = value;
}

void NBSM_SetFloat(NBSM_Value *var, float value)
{
    NBSM_Assert(var->type == NBSM_FLOAT);

    var->value.f = value;
}

void NBSM_SetBoolean(NBSM_Value *var, bool value)
{
    NBSM_Assert(var->type == NBSM_BOOLEAN);

    var->value.b = value;
}

int NBSM_GetInteger(NBSM_Value *var)
{
    NBSM_Assert(var->type == NBSM_INTEGER);

    return var->value.i;
}

float NBSM_GetFloat(NBSM_Value *var)
{
    NBSM_Assert(var->type == NBSM_FLOAT);

    return var->value.f;
}

bool NBSM_GetBoolean(NBSM_Value *var)
{
    NBSM_Assert(var->type == NBSM_BOOLEAN);

    return var->value.b;
}

NBSM_Value *NBSM_GetVariable(NBSM_Machine *machine, const char *name)
{
    return GetInHTable(machine->variables, name);
}

#ifdef NBSM_JSON_BUILDER

NBSM_MachineBuilder *NBSM_CreateBuilderFromJSON(const char *json)
{
    NBSM_MachineBuilder *builder = NBSM_Alloc(sizeof(NBSM_MachineBuilder));

    builder->state_count = 0;
    builder->states = NULL;

    builder->variable_count = 0;
    builder->variables = NULL;

    builder->transition_count = 0;
    builder->transitions = NULL;

    struct json_value_s *root = json_parse(json, strlen(json));

    NBSM_Assert(root->type == json_type_object);

    struct json_object_s *object = (struct json_object_s*)root->payload;
    struct json_object_element_s *node = object->start;

    while (node)
    {
        if (strcmp(node->name->string, "variables") == 0)
        {
            NBSM_Assert(node->value->type == json_type_array);

            LoadVariablesFromJSON(builder, node->value->payload);
        }
        else if (strcmp(node->name->string, "states") == 0)
        {
            NBSM_Assert(node->value->type == json_type_array);

            LoadStatesFromJSON(builder, node->value->payload);
        }
        else if (strcmp(node->name->string, "transitions") == 0)
        {
            NBSM_Assert(node->value->type == json_type_array);

            LoadTransitionsFromJSON(builder, node->value->payload);
        }

        node = node->next;
    }

    NBSM_Dealloc(root);

    return builder;
}

#endif // NBSM_JSON_BUILDER

void NBSM_DestroyBuilder(NBSM_MachineBuilder *builder)
{
    if (builder->states)
        NBSM_Dealloc(builder->states);

    if (builder->variables)
        NBSM_Dealloc(builder->variables);
    
    if (builder->transitions)
    {
        for (unsigned int i = 0; i < builder->transition_count; i++)
        {
            NBSM_ConditionBlueprint *conditions = builder->transitions[i].conditions;

            if (conditions)
                NBSM_Dealloc(conditions);
        }

        NBSM_Dealloc(builder->transitions);
    }

    NBSM_Dealloc(builder);
}

#pragma endregion // Public API

static void ChangeState(NBSM_Machine *machine, NBSM_State *state)
{
    NBSM_State *prev_state = machine->current;

    machine->current = state;

    if (prev_state->on_exit)
        prev_state->on_exit(machine);

    if (machine->current->on_enter)
        machine->current->on_enter(machine);
}

static NBSM_ConditionFunc GetConditionFunction(NBSM_ConditionType type)
{
    switch (type)
    {
    case NBSM_EQ:
        return ConditionEQ;

    case NBSM_NEQ:
        return ConditionNEQ;

    case NBSM_LT:
        return ConditionLT;

    case NBSM_LTE:
        return ConditionLTE;

    case NBSM_GT:
        return ConditionGT;

    case NBSM_GTE:
        return ConditionGTE;
    }
}

static bool ConditionEQ(NBSM_Value *v1, NBSM_Value *v2)
{
    NBSM_Assert(v1->type == v2->type);

    if (v1->type == NBSM_INTEGER)
        return v1->value.i == v2->value.i;

    if (v1->type == NBSM_FLOAT)
        return fabs(v1->value.f - v2->value.f) < FLT_EPSILON;

    if (v1->type == NBSM_BOOLEAN)
        return v1->value.b == v2->value.b;

    return false;
}

static bool ConditionNEQ(NBSM_Value *v1, NBSM_Value *v2)
{
    return !ConditionEQ(v1, v2);
}

static bool ConditionLT(NBSM_Value *v1, NBSM_Value *v2)
{
    NBSM_Assert(v1->type == v2->type && v1->type != NBSM_BOOLEAN);

    if (v1->type == NBSM_INTEGER)
        return v1->value.i < v2->value.i;

    if (v1->type == NBSM_FLOAT)
        return v1->value.f < v2->value.f - FLT_EPSILON;

    return false;
}

static bool ConditionLTE(NBSM_Value *v1, NBSM_Value *v2)
{
    NBSM_Assert(v1->type == v2->type && v1->type != NBSM_BOOLEAN);

    if (v1->type == NBSM_INTEGER)
        return v1->value.i <= v2->value.i;

    if (v1->type == NBSM_FLOAT)
        return v1->value.f < v2->value.f - FLT_EPSILON;

    return false;
}

static bool ConditionGT(NBSM_Value *v1, NBSM_Value *v2)
{
    NBSM_Assert(v1->type == v2->type && v1->type != NBSM_BOOLEAN);

    if (v1->type == NBSM_INTEGER)
        return v1->value.i > v2->value.i;

    if (v1->type == NBSM_FLOAT)
        return v1->value.f > v2->value.f + FLT_EPSILON;

    return false;
}

static bool ConditionGTE(NBSM_Value *v1, NBSM_Value *v2)
{
    if (v1->type == NBSM_INTEGER)
        return v1->value.i >= v2->value.i;

    if (v1->type == NBSM_FLOAT)
        return v1->value.f > v2->value.f + FLT_EPSILON;

    return false;
}

static void DestroyMachineValue(void *ptr)
{
    NBSM_Dealloc(ptr);
}

static void DestroyMachineState(void *ptr)
{
    NBSM_State *state = ptr;
    NBSM_Transition *t = state->transitions;

    while (t)
    {
        NBSM_Transition *next = t->next;

        DestroyMachineTransition(t);

        t = next;
    }

    NBSM_Dealloc(ptr);
}

static void DestroyMachineTransition(NBSM_Transition *transition)
{
    NBSM_Condition *c = transition->conditions;

    while (c)
    {
        NBSM_Condition *next = c->next;

        NBSM_Dealloc(c);

        c = next;
    }

    NBSM_Dealloc(transition);
}

#ifdef NBSM_JSON_BUILDER

static void LoadVariablesFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *var_arr)
{
    builder->variable_count = var_arr->length;
    builder->variables = NBSM_Alloc(sizeof(NBSM_VariableBlueprint) * builder->variable_count);

    struct json_array_element_s *arr_node = var_arr->start;

    int i = 0;

    while (arr_node)
    {
        NBSM_Assert(arr_node->value->type == json_type_object);

        struct json_object_s *var_obj = arr_node->value->payload;
        struct json_object_element_s *var_node = var_obj->start;
        NBSM_VariableBlueprint *var = &builder->variables[i];

        while (var_node)
        {
            if (strcmp(var_node->name->string, "name") == 0)
            {
                NBSM_Assert(var_node->value->type == json_type_string);

                var->name = ((struct json_string_s *)var_node->value->payload)->string;
            }
            else if (strcmp(var_node->name->string, "type") == 0)
            {
                NBSM_Assert(var_node->value->type == json_type_string);

                var->type = GetVariableTypeFromJSON(((struct json_string_s *)var_node->value->payload)->string);

                NBSM_Assert(var->type >= 0);
            }

            var_node = var_node->next;
        }

        arr_node = arr_node->next;
        i++;
    }
}

static void LoadStatesFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *state_arr)
{
    builder->state_count = state_arr->length;
    builder->states = NBSM_Alloc(sizeof(NBSM_StateBlueprint) * builder->state_count);

    struct json_array_element_s *arr_node = state_arr->start;

    int i = 0;

    while (arr_node)
    {
        NBSM_Assert(arr_node->value->type == json_type_object);

        struct json_object_s *state_obj = arr_node->value->payload;
        struct json_object_element_s *state_node = state_obj->start;
        NBSM_StateBlueprint *state = &builder->states[i];

        while (state_node)
        {
            if (strcmp(state_node->name->string, "name") == 0)
            {
                NBSM_Assert(state_node->value->type == json_type_string);

                state->name = ((struct json_string_s *)state_node->value->payload)->string;
            }
            else if (strcmp(state_node->name->string, "is_initial") == 0)
            {
                NBSM_Assert(state_node->value->type == json_type_true || state_node->value->type == json_type_false);

                state->is_initial = state_node->value->type == json_type_true;
            }

            state_node = state_node->next;
        }

        arr_node = arr_node->next;
        i++;
    }
}

static void LoadTransitionsFromJSON(NBSM_MachineBuilder *builder, struct json_array_s *trans_arr)
{
    builder->transition_count = trans_arr->length;
    builder->transitions = NBSM_Alloc(sizeof(NBSM_TransitionBlueprint) * builder->transition_count);

    struct json_array_element_s *arr_node = trans_arr->start;

    int i = 0;

    while (arr_node)
    {
        NBSM_Assert(arr_node->value->type == json_type_object);

        struct json_object_s *trans_obj = arr_node->value->payload;
        struct json_object_element_s *trans_node = trans_obj->start;
        NBSM_TransitionBlueprint *transition = &builder->transitions[i];

        transition->condition_count = 0;
        transition->conditions = NULL;

        while (trans_node)
        {
            if (strcmp(trans_node->name->string, "source") == 0)
            {
                NBSM_Assert(trans_node->value->type == json_type_string);

                transition->from = ((struct json_string_s *)trans_node->value->payload)->string;
            }
            else if (strcmp(trans_node->name->string, "target") == 0)
            {
                NBSM_Assert(trans_node->value->type == json_type_string);

                transition->to = ((struct json_string_s *)trans_node->value->payload)->string;
            }
            else if (strcmp(trans_node->name->string, "conditions") == 0)
            {
                NBSM_Assert(trans_node->value->type == json_type_array);

                LoadConditionsFromJSON(transition, i, trans_node->value->payload);
            }

            trans_node = trans_node->next;
        }

        arr_node = arr_node->next;
        i++;
    }
}

static void LoadConditionsFromJSON(NBSM_TransitionBlueprint *transition, unsigned int trans_idx, struct json_array_s *cond_arr)
{
    transition->condition_count = cond_arr->length;
    transition->conditions = NBSM_Alloc(sizeof(NBSM_ConditionBlueprint) * transition->condition_count);

    struct json_array_element_s *arr_node = cond_arr->start;
    int i = 0;

    while (arr_node)
    {
        NBSM_Assert(arr_node->value->type == json_type_object);

        struct json_object_s *cond_obj = arr_node->value->payload;
        struct json_object_element_s *cond_node = cond_obj->start;
        NBSM_ConditionBlueprint *cond = &transition->conditions[i];

        cond->transition_idx = trans_idx;

        while (cond_node)
        {
            if (strcmp(cond_node->name->string, "type") == 0)
            {
                NBSM_Assert(cond_node->value->type == json_type_string);

                cond->type = GetConditionTypeFromJSON(((struct json_string_s *)cond_node->value->payload)->string);

                NBSM_Assert(cond->type >= 0);
            }
            else if (strcmp(cond_node->name->string, "left_op") == 0)
            {
                NBSM_Assert(cond_node->value->type == json_type_string);

                cond->var_name = ((struct json_string_s *)cond_node->value->payload)->string;
            }
            else if (strcmp(cond_node->name->string, "right_op") == 0)
            {
                NBSM_Assert(cond_node->value->type == json_type_object);

                cond->right_op = LoadConditionOperandFromJSON(cond_node->value->payload);
            }

            cond_node = cond_node->next;
        }

        arr_node = arr_node->next;
        i++;
    }
}

static NBSM_ConditionOperandBlueprint LoadConditionOperandFromJSON(struct json_object_s *op_obj)
{
    struct json_object_element_s *op_node = op_obj->start;
    NBSM_ConditionOperandBlueprint op = { .type = -1 };

    while (op_node)
    {
        if (strcmp(op_node->name->string, "type") == 0)
        {
            NBSM_Assert(op_node->value->type == json_type_string);

            const char *op_type_str = ((struct json_string_s *)op_node->value->payload)->string;

            if (strcmp(op_type_str, "const") == 0)
                op.type = NBSM_OPERAND_CONST;
            else if (strcmp(op_type_str, "var") == 0)
                op.type = NBSM_OPERAND_VAR;

            NBSM_Assert(op.type >= 0);
        }
        else if (strcmp(op_node->name->string, "const") == 0)
        {
            NBSM_Assert(op_node->value->type == json_type_object);
            NBSM_Assert(op.type == NBSM_OPERAND_CONST);

            struct json_object_element_s *const_node = ((struct json_object_s *)op_node->value->payload)->start;

            while (const_node)
            {
                if (strcmp(const_node->name->string, "type") == 0)
                {
                    NBSM_Assert(const_node->value->type == json_type_string);

                    op.data.constant.type =
                        GetVariableTypeFromJSON(((struct json_string_s *)const_node->value->payload)->string);
                }
                else if (strcmp(const_node->name->string, "value") == 0)
                {
                    if (const_node->value->type == json_type_number)
                    {
                        NBSM_Assert(op.data.constant.type == NBSM_INTEGER || op.data.constant.type == NBSM_FLOAT);

                        const char *val_str = ((struct json_number_s *)const_node->value->payload)->number;

                        if (op.data.constant.type == NBSM_INTEGER)
                            op.data.constant.value.i = atoi(val_str);
                        else if (op.data.constant.type == NBSM_FLOAT)
                            op.data.constant.value.f = atof(val_str);
                    }
                    else if (const_node->value->type == json_type_true)
                    {
                        NBSM_Assert(op.data.constant.type == NBSM_BOOLEAN);

                        op.data.constant.value.b = true;
                    }
                    else if (const_node->value->type == json_type_false)
                    {
                        NBSM_Assert(op.data.constant.type == NBSM_BOOLEAN);

                        op.data.constant.value.b = false;
                    }
                }

                const_node = const_node->next;
            }
        }
        else if (strcmp(op_node->name->string, "var") == 0)
        {
            NBSM_Assert(op_node->value->type == json_type_string);
            NBSM_Assert(op.type == NBSM_OPERAND_VAR);

            op.data.var_name = ((struct json_string_s *)op_node->value->payload)->string;
        }

        op_node = op_node->next;
    }

    return op;
}

static NBSM_ValueType GetVariableTypeFromJSON(const char *type_str)
{
    if (strcmp(type_str, "int") == 0)
        return NBSM_INTEGER;

    if (strcmp(type_str, "float") == 0)
        return NBSM_FLOAT;

    if (strcmp(type_str, "bool") == 0)
        return NBSM_BOOLEAN;

    return -1;
}

static NBSM_ConditionType GetConditionTypeFromJSON(const char *type_str)
{
    if (strcmp(type_str, "eq") == 0)
        return NBSM_EQ;

    if (strcmp(type_str, "neq") == 0)
        return NBSM_NEQ;

    if (strcmp(type_str, "lt") == 0)
        return NBSM_LT;

    if (strcmp(type_str, "lte") == 0)
        return NBSM_LTE;

    if (strcmp(type_str, "gt") == 0)
        return NBSM_GT;

    if (strcmp(type_str, "gte") == 0)
        return NBSM_GTE;

    return -1;
}

#endif // NBSM_JSON_BUILDER

#pragma endregion // State machine

#endif // NBSM_IMPL

#pragma endregion // Implementation

#endif // NBSM
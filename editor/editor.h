#pragma once

#include <raylib.h>
#include <raymath.h>

#include "../nbsm.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

typedef struct
{
    Rectangle rect;
} StateRenderData;

typedef struct
{
    Vector2 start;
    Vector2 end;
    bool is_selected;
} TransitionRenderData;

typedef struct __EditorVariable EditorVariable;

struct __EditorVariable
{
    char *name;
    NBSM_ValueType type;
    EditorVariable *next;
    EditorVariable *prev;
};

typedef struct __EditorCondition EditorCondition;

struct __EditorCondition
{
    NBSM_ConditionType type;
    EditorVariable *var;
    NBSM_Value constant;
    EditorCondition *next;
    EditorCondition *prev;
};

typedef struct __EditorState EditorState;
typedef struct __EditorTransition EditorTransition;

struct __EditorTransition
{
    EditorState *src_state;
    EditorState *target_state;
    TransitionRenderData render_data;

    EditorCondition *conditions;
    unsigned int condition_count;

    EditorTransition *next;
    EditorTransition *prev;
};

struct __EditorState
{
    char *name;
    StateRenderData render_data;

    EditorTransition *transitions;
    unsigned int transition_count;

    bool is_initial;

    EditorState *next;
    EditorState *prev;
};

typedef struct
{
    EditorState *states;
    unsigned int state_count;
    EditorVariable *variables;
    unsigned int variable_count;
} EditorStateMachine;

void InitStateMachine(EditorStateMachine *machine);
void DeinitStateMachine(EditorStateMachine *machine);
EditorState *AddStateToMachine(EditorStateMachine *machine, char *name);
void RemoveStateFromMachine(EditorStateMachine *machine, EditorState *state);
EditorTransition *AddTransitionToState(EditorState *from, EditorState *to);
void RemoveTransitionFromState(EditorTransition *trans);
EditorVariable *AddVariableToMachine(EditorStateMachine *machine, char *name, NBSM_ValueType type);
void RemoveVariableFromMachine(EditorStateMachine *machine, EditorVariable *var);
EditorCondition *AddConditionToTransition(
        EditorTransition *trans,
        NBSM_ConditionType type,
        EditorVariable *var,
        NBSM_Value constant);
void RemoveConditionFromTransition(EditorTransition *trans, EditorCondition *cond);
EditorState *FindStateByName(EditorStateMachine *machine, const char *name);
EditorVariable *FindVariableByName(EditorStateMachine *machine, const char *name);
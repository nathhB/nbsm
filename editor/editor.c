/*
   Copyright (C) 2021 BIAGINI Nathan

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source distribution.

*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "editor.h"

void InitStateMachine(EditorStateMachine *machine)
{
    machine->states = NULL;
    machine->state_count = 0;
    machine->variables = NULL;
    machine->variable_count = 0;
}

void DeinitStateMachine(EditorStateMachine *machine)
{
    // TODO
}

EditorState *AddStateToMachine(EditorStateMachine *machine, char *name)
{
    EditorState *state = malloc(sizeof(EditorState));

    state->name = name;
    state->is_initial = false;
    state->transitions = NULL;
    state->transition_count = 0;

    if (!machine->states)
    {
        state->prev = NULL;
        state->next = NULL;

        machine->states = state;
    }
    else
    {
        EditorState *s = machine->states;

        while (s->next)
            s = s->next;

        assert(!s->next);

        state->prev = s;
        state->next = NULL;

        s->next = state;
    }

    machine->state_count++;

    return state;
}

void RemoveStateFromMachine(EditorStateMachine *machine, EditorState *state)
{
    if (!state->prev)
    {
        // first item of the list
        machine->states = state->next;

        if (state->next)
            state->next->prev = NULL;
    }
    else
    {
        state->prev->next = state->next;
        
        if (state->next)
            state->next->prev = state->prev;
    }

    machine->state_count--;

    free(state->name);
    free(state);
}

EditorTransition *AddTransitionToState(EditorState *from, EditorState *to)
{
    EditorTransition *trans = malloc(sizeof(EditorTransition));

    trans->src_state = from;
    trans->target_state = to;
    trans->conditions = NULL;
    trans->condition_count = 0;

    if (!from->transitions)
    {
        trans->prev = NULL;
        trans->next = NULL;

        from->transitions = trans;
    }
    else
    {
        EditorTransition *t = from->transitions;

        while (t->next)
            t = t->next;

        assert(!t->next);

        trans->prev = t;
        trans->next = NULL;

        t->next = trans;
    }

    from->transition_count++;

    return trans;
}

void RemoveTransitionFromState(EditorTransition *trans)
{
    EditorState *from = trans->src_state;

    if (!trans->prev)
    {
        // first item of the list
        from->transitions = trans->next;

        if (trans->next)
            trans->next->prev = NULL;
    }
    else
    {
        trans->prev->next = trans->next;
        
        if (trans->next)
            trans->next->prev = trans->prev;
    }

    // remove all conditions

    EditorCondition *cond = trans->conditions;

    while (cond)
    {
        EditorCondition *next = cond->next;

        RemoveConditionFromTransition(trans, cond);

        cond = next;
    }

    free(trans);

    from->transition_count--;
}

EditorVariable *AddVariableToMachine(EditorStateMachine *machine, char *name, NBSM_ValueType type)
{
    EditorVariable *var = malloc(sizeof(EditorVariable));

    var->name = name;
    var->type = type;

    if (!machine->variables)
    {
        var->prev = NULL;
        var->next = NULL;

        machine->variables = var;
    }
    else
    {
        EditorVariable *v = machine->variables;

        while (v->next)
            v = v->next;

        assert(!v->next);

        var->prev = v;
        var->next = NULL;

        v->next = var;
    }

    machine->variable_count++;

    return var;
}

void RemoveVariableFromMachine(EditorStateMachine *machine, EditorVariable *var)
{
    if (!var->prev)
    {
        // first item of the list
        machine->variables = var->next;

        if (var->next)
            var->next->prev = NULL;
    }
    else
    {
        var->prev->next = var->next;
        
        if (var->next)
            var->next->prev = var->prev;
    }

    free(var->name);
    free(var);

    machine->variable_count--;
}

EditorCondition *AddConditionToTransition(
        EditorTransition *trans,
        NBSM_ConditionType type,
        EditorVariable *left_op,
        NBSM_ConditionOperandBlueprint right_op)
{
    EditorCondition *cond = malloc(sizeof(EditorCondition));

    cond->type = type;
    cond->left_op = left_op;
    cond->right_op = right_op;

    if (!trans->conditions)
    {
        cond->prev = NULL;
        cond->next = NULL;

        trans->conditions = cond;
    }
    else
    {
        EditorCondition *c = trans->conditions;

        while (c->next)
            c = c->next;

        assert(!c->next);

        cond->prev = c;
        cond->next = NULL;

        c->next = cond;
    }

    trans->condition_count++;

    return cond;
}

void RemoveConditionFromTransition(EditorTransition *trans, EditorCondition *cond)
{
    if (!cond->prev)
    {
        // first item of the list
        trans->conditions = cond->next;

        if (cond->next)
            cond->next->prev = NULL;
    }
    else
    {
        cond->prev->next = cond->next;
        
        if (cond->next)
            cond->next->prev = cond->prev;
    }

    free(cond);

    trans->condition_count--;
}

EditorState *FindStateByName(EditorStateMachine *machine, const char *name)
{
    EditorState *state = machine->states;

    while (state)
    {
        if (strcmp(state->name, name) == 0)
            return state;

        state = state->next;
    }

    return NULL;
}

EditorVariable *FindVariableByName(EditorStateMachine *machine, const char *name)
{
    EditorVariable *var = machine->variables;

    while (var)
    {
        if (strcmp(var->name, name) == 0)
            return var;

        var = var->next;
    }

    return NULL;
}
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

#include <stdio.h>
#include <assert.h>
#include <json.h>

#include "editor.h"

static void LoadVariables(EditorStateMachine *machine, json_object *obj);
static void LoadStates(EditorStateMachine *machine, json_object *obj);
static void LoadTransitions(EditorStateMachine *machine, json_object *obj);
static void LoadConditions(EditorStateMachine *machine, EditorTransition *trans, json_object *trans_obj);

static json_object *MachineToJSON(EditorStateMachine *machine);
static void VariablesToJSON(EditorStateMachine *machine, json_object *obj);
static void StatesToJSON(EditorStateMachine *machine, json_object *obj);
static void TransitionsToJSON(EditorStateMachine *machine, json_object *obj);
static json_object *BuildTransitionConditionArray(EditorTransition *trans);

// JS API

char *js_open_file_dialog(void);
void js_download(const char *file_name, const char *file_content);

// ------

static const char *json_var_types[] = {
    "int",
    "float",
    "bool"};

static const char *json_cond_types[] = {
    "eq",
    "neq",
    "lt",
    "lte",
    "gt",
    "gte"};

int Load(EditorStateMachine *machine, char *data)
{
    json_object *root = json_tokener_parse(data ? data : "{}");

    if (!root)
        return -1;

    InitStateMachine(machine);

    LoadVariables(machine, root);
    LoadStates(machine, root);
    LoadTransitions(machine, root);

    json_object_put(root);

    return 0;
}

#ifdef EMSCRIPTEN

void Download(EditorStateMachine *machine, const char *file_name)
{
    json_object *root = MachineToJSON(machine);
    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);

    json_object_put(root);

    char *file_name_ext = strdup(file_name);

    strncat(file_name_ext, ".json", 255 - strlen(file_name_ext) - 1);

    js_download(file_name_ext, json);
    free(file_name_ext);
}

#else

int Save(EditorStateMachine *machine, const char *path)
{
    json_object *root = MachineToJSON(machine);
    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);

    json_object_put(root);

    FILE *file = fopen(path, "w");

    if (!file)
        return -1;

    int ret = fputs(json, file);

    if (ret < 0)
        return -1;

    fclose(file);

    return 0;
}

#endif // EMSCRIPTEN

char *OpenFileDialog(void)
{
#ifdef EMSCRIPTEN
    return js_open_file_dialog();
#else
    abort(); // unsupported for now

    return NULL;
#endif
}

char *ReadFileContent(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);

    long size = ftell(f);

    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);

    fread(content, 1, size, f);
    fclose(f);

    content[size] = 0;

    return content;
}

static void LoadVariables(EditorStateMachine *machine, json_object *obj)
{
    json_object *var_arr_obj = json_object_object_get(obj, "variables");

    if (!var_arr_obj)
        return;

    unsigned int var_count = json_object_array_length(var_arr_obj);

    for (unsigned int i = 0; i < var_count; i++)
    {
        json_object *var_obj = json_object_array_get_idx(var_arr_obj, i);
        json_object *name_obj = json_object_object_get(var_obj, "name");
        json_object *type_obj = json_object_object_get(var_obj, "type");
        const char *type_str = json_object_get_string(type_obj);

        NBSM_ValueType type = 0;

        for (; strcmp(type_str, json_var_types[type]) != 0 && type <= NBSM_BOOLEAN; type++);

        AddVariableToMachine(machine, strdup(json_object_get_string(name_obj)), type);
    }
}

static void LoadStates(EditorStateMachine *machine, json_object *obj)
{
    json_object *state_arr_obj = json_object_object_get(obj, "states");

    if (!state_arr_obj)
        return;

    unsigned int state_count = json_object_array_length(state_arr_obj);

    for (unsigned int i = 0; i < state_count; i++)
    {
        json_object *state_obj = json_object_array_get_idx(state_arr_obj, i);
        json_object *name_obj = json_object_object_get(state_obj, "name");
        json_object *is_initial_obj = json_object_object_get(state_obj, "is_initial");
        json_object *rect_obj = json_object_object_get(state_obj, "rect");
        json_object *x_obj = json_object_array_get_idx(rect_obj, 0);
        json_object *y_obj = json_object_array_get_idx(rect_obj, 1);
        json_object *w_obj = json_object_array_get_idx(rect_obj, 2);
        json_object *h_obj = json_object_array_get_idx(rect_obj, 3);

        EditorState *state = AddStateToMachine(machine, strdup(json_object_get_string(name_obj)));

        state->is_initial = json_object_get_boolean(is_initial_obj);
        state->render_data.rect = (Rectangle){
            json_object_get_int(x_obj),
            json_object_get_int(y_obj),
            json_object_get_int(w_obj),
            json_object_get_int(h_obj)};
    }
}

static void LoadTransitions(EditorStateMachine *machine, json_object *obj)
{
    json_object *trans_arr_obj = json_object_object_get(obj, "transitions");

    if (!trans_arr_obj)
        return;

    unsigned int transition_count = json_object_array_length(trans_arr_obj);

    for (unsigned int i = 0; i < transition_count; i++)
    {
        json_object *trans_obj = json_object_array_get_idx(trans_arr_obj, i);
        json_object *source_obj = json_object_object_get(trans_obj, "source");
        json_object *target_obj = json_object_object_get(trans_obj, "target");
        json_object *start_obj = json_object_object_get(trans_obj, "start");
        json_object *end_obj = json_object_object_get(trans_obj, "end");
        json_object *start_x_obj = json_object_array_get_idx(start_obj, 0);
        json_object *start_y_obj = json_object_array_get_idx(start_obj, 1);
        json_object *end_x_obj = json_object_array_get_idx(end_obj, 0);
        json_object *end_y_obj = json_object_array_get_idx(end_obj, 1);

        EditorState *source = FindStateByName(machine, json_object_get_string(source_obj));
        EditorState *target = FindStateByName(machine, json_object_get_string(target_obj));

        assert(source);
        assert(target);

        EditorTransition *trans = AddTransitionToState(source, target);

        LoadConditions(machine, trans, trans_obj);

        trans->render_data.start = (Vector2){json_object_get_int(start_x_obj), json_object_get_int(start_y_obj)};
        trans->render_data.end = (Vector2){json_object_get_int(end_x_obj), json_object_get_int(end_y_obj)};
    }
}

static void LoadConditions(EditorStateMachine *machine, EditorTransition *trans, json_object *trans_obj)
{
    json_object *cond_arr_obj = json_object_object_get(trans_obj, "conditions");

    if (!cond_arr_obj)
        return;

    unsigned int condition_count = json_object_array_length(cond_arr_obj);

    for (unsigned int i = 0; i < condition_count; i++)
    {
        json_object *cond_obj = json_object_array_get_idx(cond_arr_obj, i);
        json_object *type_obj = json_object_object_get(cond_obj, "type");
        json_object *var_name_obj = json_object_object_get(cond_obj, "var_name");
        json_object *var_type_obj = json_object_object_get(cond_obj, "var_type");
        json_object *const_val_obj = json_object_object_get(cond_obj, "const_val");

        EditorVariable *var = FindVariableByName(machine, json_object_get_string(var_name_obj));

        assert(var);

        const char *type_str = json_object_get_string(var_type_obj);
        NBSM_ValueType val_type = 0;

        for (; strcmp(type_str, json_var_types[val_type]) != 0 && val_type <= NBSM_BOOLEAN; val_type++);

        assert(var->type == val_type);

        NBSM_Value constant = { .type = val_type };

        if (val_type == NBSM_BOOLEAN)
            constant.value.b = json_object_get_boolean(const_val_obj);
        else if (val_type == NBSM_INTEGER)
            constant.value.i = json_object_get_int(const_val_obj);
        else if (val_type == NBSM_FLOAT)
            constant.value.f = json_object_get_double(const_val_obj);

        const char *cond_type_str = json_object_get_string(type_obj);
        NBSM_ConditionType cond_type = 0;

        for (; strcmp(cond_type_str, json_cond_types[cond_type]) != 0 && cond_type <= NBSM_GTE; cond_type++);

        AddConditionToTransition(trans, cond_type, var, constant);
    }
}

static json_object *MachineToJSON(EditorStateMachine *machine)
{
    json_object *root = json_object_new_object();

    VariablesToJSON(machine, root);
    StatesToJSON(machine, root);
    TransitionsToJSON(machine, root);

    return root;
}

static void VariablesToJSON(EditorStateMachine *machine, json_object *obj)
{
    EditorVariable *var = machine->variables;

    json_object *var_arr_obj = json_object_new_array_ext(machine->variable_count);

    while (var)
    {
        json_object *var_obj = json_object_new_object();

        json_object_object_add(var_obj, "name", json_object_new_string(var->name));
        json_object_object_add(var_obj, "type", json_object_new_string(json_var_types[var->type]));

        json_object_array_add(var_arr_obj, var_obj);

        var = var->next;
    }

    json_object_object_add(obj, "variables", var_arr_obj);
}

static void StatesToJSON(EditorStateMachine *machine, json_object *obj)
{
    EditorState *state = machine->states;

    json_object *state_arr_obj = json_object_new_array_ext(machine->state_count);

    while (state)
    {
        json_object *state_obj = json_object_new_object();
        json_object *rect_arr_obj = json_object_new_array_ext(4);
        Rectangle rect = state->render_data.rect;

        json_object_array_add(rect_arr_obj, json_object_new_int((int)rect.x));
        json_object_array_add(rect_arr_obj, json_object_new_int((int)rect.y));
        json_object_array_add(rect_arr_obj, json_object_new_int((int)rect.width));
        json_object_array_add(rect_arr_obj, json_object_new_int((int)rect.height));

        json_object_object_add(state_obj, "name", json_object_new_string(state->name));
        json_object_object_add(state_obj, "is_initial", json_object_new_boolean(state->is_initial));
        json_object_object_add(state_obj, "rect", rect_arr_obj);

        json_object_array_add(state_arr_obj, state_obj);

        state = state->next;
    }

    json_object_object_add(obj, "states", state_arr_obj);
}

static void TransitionsToJSON(EditorStateMachine *machine, json_object *obj)
{
    EditorState *state = machine->states;
    json_object *trans_arr_obj = json_object_new_array();

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            json_object *trans_obj = json_object_new_object();
            json_object *start_arr_obj = json_object_new_array_ext(2);
            json_object *end_arr_obj = json_object_new_array_ext(2);
            Vector2 start = trans->render_data.start;
            Vector2 end = trans->render_data.end;

            json_object_array_add(start_arr_obj, json_object_new_int((int)start.x));
            json_object_array_add(start_arr_obj, json_object_new_int((int)start.y));

            json_object_array_add(end_arr_obj, json_object_new_int((int)end.x));
            json_object_array_add(end_arr_obj, json_object_new_int((int)end.y));

            json_object_object_add(trans_obj, "source", json_object_new_string(trans->src_state->name));
            json_object_object_add(trans_obj, "target", json_object_new_string(trans->target_state->name));
            json_object_object_add(trans_obj, "conditions", BuildTransitionConditionArray(trans));
            json_object_object_add(trans_obj, "start", start_arr_obj);
            json_object_object_add(trans_obj, "end", end_arr_obj);

            json_object_array_add(trans_arr_obj, trans_obj);

            trans = trans->next;
        }

        state = state->next;
    }

    json_object_object_add(obj, "transitions", trans_arr_obj);
}

static json_object *BuildTransitionConditionArray(EditorTransition *trans)
{
    json_object *arr_obj = json_object_new_array_ext(trans->condition_count);
    EditorCondition *cond = trans->conditions;

    while (cond)
    {
        json_object *cond_obj = json_object_new_object();

        json_object_object_add(cond_obj, "type", json_object_new_string(json_cond_types[cond->type]));
        json_object_object_add(cond_obj, "var_name", json_object_new_string(cond->var->name));
        json_object_object_add(cond_obj, "var_type", json_object_new_string(json_var_types[cond->var->type]));

        json_object *const_val_obj = NULL;

        if (cond->var->type == NBSM_BOOLEAN)
            const_val_obj = json_object_new_boolean(cond->constant.value.b);
        else if (cond->var->type == NBSM_INTEGER)
            const_val_obj = json_object_new_int(cond->constant.value.i);
        else if (cond->var->type == NBSM_FLOAT)
        {
            char f_str[255] = {0};

            snprintf(f_str, sizeof(f_str), "%.2f", cond->constant.value.f);

            const_val_obj = json_object_new_double_s(cond->constant.value.f, f_str);
        }

        json_object_object_add(cond_obj, "const_val", const_val_obj);

        json_object_array_add(arr_obj, cond_obj);

        cond = cond->next;
    }

    return arr_obj;
}

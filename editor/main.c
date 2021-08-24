#include <stdio.h>
#include <stdbool.h>

#ifdef EMSCRIPTEN

// when compiling with emcc, we defines our own isnumber function

#include <stdlib.h>

static bool isnumber(const char *s)
{
    char *e = NULL;

    (void)strtol(s, &e, 0);

    return e != NULL && *e == (char)0;
}

// the JS API

int js_get_win_width(void);
int js_get_win_height(void);

#else

#include <ctype.h> // isnumber

#endif // EMSCRIPTEN

#define RAYGUI_IMPLEMENTATION

#include "raygui.h"

#include "editor.h"
#include "save_load.h"

#ifndef EMSCRIPTEN

#define VIEWPORT_WIDTH 1024
#define VIEWPORT_HEIGHT 800

#else

#define DOWNLOAD_WIN_WIDTH 350
#define DOWNLOAD_WIN_HEIGHT 120

#endif // EMSCRIPTEN

#define CELL_SIZE 20
#define GRID_Y_OFFSET (CELL_SIZE * 2)
#define WINDOW_BACKGROUND_COLOR LIGHTGRAY
#define GRID_COLOR ((Color){102, 102, 102, 100})
#define MIN_STATE_WIDTH (CELL_SIZE*6)
#define NEW_STATE_WIN_WIDTH 300
#define NEW_STATE_WIN_HEIGHT 80
#define VARIABLES_WIN_WIDTH 405
#define VARIABLES_WIN_HEIGHT 350
#define NEW_VARIABLE_WIN_WIDTH 350
#define NEW_VARIABLE_WIN_HEIGHT 70
#define NEW_CONDITION_WIN_WIDTH 440
#define NEW_CONDITION_WIN_HEIGHT 70
#define EDIT_TRANSITION_WIN_WIDTH 480
#define EDIT_TRANSITION_WIN_HEIGHT 350
#define ALERT_WIN_HEIGHT 60
#define TRANSITION_CIRCLE_RADIUS 3
#define TRANSITION_LINE_COLOR ((Color){46, 117, 136, 255})
#define TRANSITION_SELECTED_LINE_COLOR ((Color){94, 190, 189, 255})
#define ALERT_TEXT_COLOR ((Color){104, 104, 104, 255})
#define TRANSITION_ARROW_SIZE 10
#define MAX_VAR_NAME_LENGTH 32

typedef enum
{
    DEFAULT_STATE,
    DRAGGING_STATE,
    NEW_STATE,
    NEW_TRANSITION,
    EDIT_VARIABLES,
    NEW_VARIABLE,
    EDIT_TRANSITION,
    NEW_CONDITION,
    CANNOT_REMOVE_VAR,
    CANNOT_CREATE_VAR,
    CANNOT_CREATE_STATE,
    CANNOT_LOAD_FILE,
    CANNOT_SAVE_FILE,

#ifdef EMSCRIPTEN
    DOWNLOAD_FILE
#endif // EMSCRIPTEN
} UIState;

EditorStateMachine machine;

static EditorState *GetSelectedState(void);
static EditorTransition *GetSelectedTransition(Vector2 pos);
static EditorState *GetTransitionOveredState(Vector2 grid_pos);
static void DoGUI(void);
static void DoNewStateWindow(void);
static void DoEditVariablesGUI(void);
static void DoNewVariableGUI(void);
static void DoNewConditionGUI(void);
static void DoEditTransitionGUI(void);
static void DoNewTransitionGUI(void);
static void DoDraggingStateGUI(void);
static void DoDefaultStateGUI(void);
static void DoCannotRemoveVarGUI(void);
static void DoCannotCreateVarGUI(void);
static void DoCannotCreateStateGUI(void);
static void DoAlertGUI(const char *text, UIState prev_state);
static void DoGrid(void);
static void DoStatesGUI(void);
static void DoTransitionsGUI(void);
static void ToggleInitialState(EditorState *initial_state);
static Vector2 SnapPositionToGrid(Vector2 v);
static void SetStyle(void);
static unsigned int ComputeStateWidth(EditorState *state);
static void RemoveState(EditorState *state);
static void RemoveAllStateRelatedTransitions(EditorState *state);
static void UpdateAllStateRelatedTransitionPositions(EditorState *state, Vector2 move);
static const char *GetTypeName(NBSM_ValueType type);
static const char *GetConditionTypeName(NBSM_ConditionType type);
static void GetValueStr(char *val_str, NBSM_Value val);
static Vector2 GetTransitionArrowPos(EditorTransition *trans);
static void ResetTransitionSelection(void);
static char *BuildDropdownVariablesString(void);
static EditorVariable *GetDropdownSelectedVariable(void);
static bool IsValueInputValid(void);
static bool IsVariableUsedByAnyCondition(EditorVariable *var);
static void UpdateConditionsConstantType(EditorVariable *var, NBSM_ValueType type);
static int GetVariableIndex(EditorVariable *var);

#ifdef EMSCRIPTEN
void DoDownloadGUI(void);
#endif // EMSCRIPTEN

static Rectangle viewport; 
static UIState ui_state = DEFAULT_STATE;
static EditorState *selected_state = NULL;
static EditorTransition *selected_trans = NULL;
static Vector2 drag_offset;
static char state_name[255] = {0};
static char var_name[MAX_VAR_NAME_LENGTH] = {0};
static Vector2 transition_start_point;
static EditorState *transition_start_state;
static RenderTexture2D arrow_texture;
static RenderTexture2D selected_arrow_texture;
static Vector2 scroll; // used to store scroll panels scrolling amount
static int dropdown_active = 0;
static int dropdown2_active = 0;
static bool dropdown_edit_mode = false;
static bool dropdown2_edit_mode = false;
static EditorVariable *edited_var = NULL;
static EditorCondition *edited_cond = NULL;
static char *vars_str = NULL;
static NBSM_Value input_value; // used to store inputed values
static char val_str[255] = {0}; // numeric values input
static const char *file_path;
static EditorVariable *rm_var;

#ifdef EMSCRIPTEN

static char download_file_name[255] = {0};

#endif // EMSCRIPTEN

int main(int argc, char **argv)
{
#ifndef EMSCRIPTEN
    // accept a file to load as an argument when not running in a web browser

    if (argc != 2)
    {
        printf("Usage: nbsm_editor [PATH]\n");

        return 1;
    }

    file_path = argv[1];

    char *file_content = ReadFileContent(file_path);

    if (Load(&machine, file_content) < 0)
    {
        printf("Failed to load file: %s\n", file_path);

        return 1;
    }

    free(file_content);
#endif // EMSCRIPTEN

#ifdef EMSCRIPTEN
    viewport = (Rectangle){0, GRID_Y_OFFSET, js_get_win_width(), js_get_win_height() - GRID_Y_OFFSET};
#else
    viewport = (Rectangle){0, GRID_Y_OFFSET, VIEWPORT_WIDTH, VIEWPORT_HEIGHT - GRID_Y_OFFSET};
#endif // EMSCRIPTEN

#ifdef __APPLE__
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
#endif // __APPLE__

    InitWindow(viewport.width, viewport.height, "nbsm editor");

    SetStyle();

    arrow_texture = LoadRenderTexture(TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE);
    selected_arrow_texture = LoadRenderTexture(TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE);

    BeginTextureMode(arrow_texture);
        ClearBackground((Color){255, 255, 255, 0});
        DrawTriangle(
            (Vector2){TRANSITION_ARROW_SIZE / 2, 0},
            (Vector2){0, TRANSITION_ARROW_SIZE},
            (Vector2){TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE},
            TRANSITION_LINE_COLOR);
    EndTextureMode();

    BeginTextureMode(selected_arrow_texture);
        ClearBackground((Color){255, 255, 255, 0});
        DrawTriangle(
            (Vector2){TRANSITION_ARROW_SIZE / 2, 0},
            (Vector2){0, TRANSITION_ARROW_SIZE},
            (Vector2){TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE},
            TRANSITION_SELECTED_LINE_COLOR);
    EndTextureMode();

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(WINDOW_BACKGROUND_COLOR);
            DoGUI();
        EndDrawing();
    }

    UnloadRenderTexture(arrow_texture);
    UnloadRenderTexture(selected_arrow_texture);
    CloseWindow();

    DeinitStateMachine(&machine);

    return 0;
}

static EditorState *GetSelectedState(void)
{
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);
    Vector2 pos = GetMousePosition();

    EditorState *state = machine.states;

    while (state)
    {
        // only consider selection when clicking in the status bar and not on the close button

        Rectangle rect = {state->render_data.rect.x + 10,
                          state->render_data.rect.y + 10,
                          state->render_data.rect.width - 30,
                          status_bar_h};

        if (CheckCollisionPointRec(pos, rect))
            return state;

        state = state->next;
    }

    return NULL;
}

static EditorTransition *GetSelectedTransition(Vector2 pos)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            Vector2 arrow_pos = GetTransitionArrowPos(trans);
            Rectangle rect = {
                arrow_pos.x - arrow_texture.texture.width / 2,
                arrow_pos.y - arrow_texture.texture.height / 2,
                arrow_texture.texture.width,
                arrow_texture.texture.height
            };

            if (CheckCollisionPointRec(pos, rect))
                return trans;

            trans = trans->next;
        }

        state = state->next;
    }

    return NULL;
}

static EditorState *GetTransitionOveredState(Vector2 grid_pos)
{
    EditorState *state = machine.states;

    while (state)
    {
        if (!transition_start_state || state != transition_start_state)
        {
            Rectangle rect = state->render_data.rect;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y, rect.width, 1}))
                return state;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y + rect.height, rect.width, 1}))
                return state;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y, 1, rect.height}))
                return state;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x + rect.width, rect.y, 1, rect.height}))
                return state;
        }

        state = state->next;
    }

    return NULL;
}

static void DoGUI(void)
{
    bool gui_locked = ui_state != DEFAULT_STATE;

    if (gui_locked)
        GuiLock();

#ifndef EMSCRIPTEN // opening a file is only supported when running in a web browser for now
    GuiDisable();
#endif // EMSCRIPTEN

    if (GuiButton((Rectangle){10, 10, 80, 20}, "Open"))
    {
        char *file_content = OpenFileDialog();
        int ret = Load(&machine, file_content);

        if (ret < 0)
            ui_state = CANNOT_LOAD_FILE;

        free(file_content);
    }

#ifndef EMSCRIPTEN
    GuiEnable();
#endif // EMSCRIPTEN

    
#ifdef EMSCRIPTEN
    if (GuiButton((Rectangle){100, 10, 120, 20}, "Download"))
    {
        ui_state = DOWNLOAD_FILE;
#else
    if (GuiButton((Rectangle){100, 10, 80, 20}, "Save"))
    {
        if (Save(&machine, file_path) < 0)
            ui_state = CANNOT_SAVE_FILE;
#endif // EMSCRIPTEN
    }

    if (GuiButton((Rectangle){viewport.width - 110, 10, 100, 20}, "New state"))
    {
        ui_state = NEW_STATE;

        return;
    }

    if (GuiButton((Rectangle){viewport.width - 220, 10, 100, 20}, "Variables"))
    {
        ui_state = EDIT_VARIABLES;

        return;
    }

    DoGrid();
    DoStatesGUI(); 
    DoTransitionsGUI();

    GuiUnlock();

    if (ui_state == NEW_STATE)
        DoNewStateWindow(); 
    else if (ui_state == NEW_TRANSITION)
        DoNewTransitionGUI();
    else if (ui_state == DRAGGING_STATE)
        DoDraggingStateGUI();
    else if (ui_state == EDIT_VARIABLES)
        DoEditVariablesGUI();
    else if (ui_state == NEW_VARIABLE)
        DoNewVariableGUI();
    else if (ui_state == EDIT_TRANSITION)
        DoEditTransitionGUI();
    else if (ui_state == NEW_CONDITION)
        DoNewConditionGUI();
    else if (ui_state == CANNOT_REMOVE_VAR)
        DoCannotRemoveVarGUI();
    else if (ui_state == CANNOT_CREATE_VAR)
        DoCannotCreateVarGUI();
    else if (ui_state == CANNOT_CREATE_STATE)
        DoCannotCreateStateGUI();
    else if (ui_state == CANNOT_LOAD_FILE)
        DoAlertGUI("Cannot load file", DEFAULT_STATE);
    else if (ui_state == CANNOT_SAVE_FILE)
        DoAlertGUI("Failed to save file", DEFAULT_STATE);
#ifdef EMSCRIPTEN
    else if (ui_state == DOWNLOAD_FILE)
        DoDownloadGUI();
#endif // EMSCRIPTEN
    else if (ui_state == DEFAULT_STATE)
        DoDefaultStateGUI();
}

static void DoNewStateWindow(void)
{
    Rectangle rect = {viewport.width / 2 - NEW_STATE_WIN_WIDTH / 2,
                      viewport.height / 2 + viewport.y - NEW_STATE_WIN_HEIGHT / 2,
                      NEW_STATE_WIN_WIDTH, NEW_STATE_WIN_HEIGHT};
    
    if (GuiWindowBox(rect, "New state"))
    {
        memset(state_name, 0, sizeof(state_name));
        ui_state = DEFAULT_STATE;

        return;
    }

    GuiTextBox((Rectangle){rect.x + 10, rect.y + 40, rect.width - 80, 20}, state_name, 255, true);

    if (strlen(state_name) < 1)
        GuiDisable();

    if (GuiButton((Rectangle){rect.x + rect.width - 60, rect.y + 40, 50, 20}, "Create"))
    {
        if (FindStateByName(&machine, state_name))
        {
            ui_state = CANNOT_CREATE_STATE;

            return;
        }
        else
        {
            EditorState *state = AddStateToMachine(&machine, strdup(state_name));
            unsigned int width = ComputeStateWidth(state);
            unsigned int height = CELL_SIZE * 4;
            Vector2 pos = {viewport.width / 2 - width / 2, viewport.height / 2 - height / 2};

            state->render_data.rect = (Rectangle){
                ((int)pos.x / CELL_SIZE) * CELL_SIZE,
                ((int)pos.y / CELL_SIZE) * CELL_SIZE,
                width,
                height};

            memset(state_name, 0, sizeof(state_name));

            ui_state = DEFAULT_STATE;
        }
    }

    GuiEnable();
}

static void DoEditVariablesGUI(void)
{
    Rectangle rect = {viewport.width / 2 - VARIABLES_WIN_WIDTH / 2,
                      viewport.height / 2 + viewport.y - VARIABLES_WIN_HEIGHT / 2,
                      VARIABLES_WIN_WIDTH, VARIABLES_WIN_HEIGHT};

    if (GuiWindowBox(rect, "Variables"))
    {
        ui_state = DEFAULT_STATE;

        return;
    }

    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2 * GuiGetStyle(STATUSBAR, BORDER_WIDTH);
    int item_height = 25;
    unsigned int item_count = machine.variable_count;
    Rectangle scroll_pan_rect = (Rectangle){rect.x, rect.y + (status_bar_h - 2) + 30, rect.width, rect.height - status_bar_h - 30};
    Rectangle content_rect = {0, 0, rect.width - 10, item_count * item_height + 5};
    Rectangle view = GuiScrollPanel(scroll_pan_rect, content_rect, &scroll); 

    if (GuiButton((Rectangle){ rect.x + rect.width - 50, rect.y + status_bar_h + 5, 40, 20 }, "Add"))
    {
       ui_state = NEW_VARIABLE;

       return;
    }

#ifndef __APPLE__
    BeginScissorMode(view.x, view.y, view.width, view.height);
#endif // __APPLE__

    EditorVariable *var = machine.variables;
    unsigned int i = 0;

    while (var)
    {
        EditorVariable *next = var->next;

        GuiTextBox(
            (Rectangle){rect.x + 17, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 150, 20},
            var->name,
            MAX_VAR_NAME_LENGTH,
            false);

        const char *type_name = GetTypeName(var->type);

        GuiTextBox(
            (Rectangle){rect.x + 177, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 100, 20},
            type_name,
            sizeof(type_name),
            false);

        if (GuiButton((Rectangle){rect.x + 287, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 50, 20}, "Edit"))
        {
            edited_var = var;
            dropdown_active = var->type; 
            ui_state = NEW_VARIABLE;

            strncpy(var_name, edited_var->name, MAX_VAR_NAME_LENGTH);
        }

        Rectangle delete_btn_rect = {
            rect.x + rect.width - 60, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 50, 20};

        if (GuiButton(delete_btn_rect, "Delete"))
        {
            if (IsVariableUsedByAnyCondition(var))
            {
                rm_var = var;
                ui_state = CANNOT_REMOVE_VAR;
                return;
            }

            RemoveVariableFromMachine(&machine, var);
        }

        i++;
        var = next;
    }

#ifndef __APPLE__
    EndScissorMode();
#endif // __APPLE__
}

static void DoNewVariableGUI(void)
{
    Rectangle rect = {viewport.width / 2 - NEW_VARIABLE_WIN_WIDTH / 2,
                      viewport.height / 2 + viewport.y - NEW_VARIABLE_WIN_HEIGHT / 2,
                      NEW_VARIABLE_WIN_WIDTH, NEW_VARIABLE_WIN_HEIGHT};

    char title[255] = {0};

    if (edited_var)
        snprintf(title, sizeof(title), "Edit variable: %s", edited_var->name);
    else
        strncpy(title, "New variable", sizeof(title));

    if (GuiWindowBox(rect, title)) {
        ui_state = EDIT_VARIABLES;
        edited_var = NULL;

        memset(var_name, 0, sizeof(var_name)); 

        return;
    }

    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2 * GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    GuiTextBox(
        (Rectangle){rect.x + 10, rect.y + status_bar_h + 10, 150, 20},
        var_name,
        MAX_VAR_NAME_LENGTH,
        true); 

    if (strlen(var_name) < 1)
        GuiDisable();

    char *btn_name = edited_var ? "Save" : "Create";

    if (GuiButton((Rectangle){rect.x + rect.width - 90, rect.y + status_bar_h + 10, 80, 20}, btn_name))
    {
        if (edited_var)
        {
            memset(edited_var->name, 0, MAX_VAR_NAME_LENGTH);
            strncpy(edited_var->name, var_name, MAX_VAR_NAME_LENGTH);

            if (dropdown_active != edited_var->type && IsVariableUsedByAnyCondition(edited_var))
                UpdateConditionsConstantType(edited_var, dropdown_active);

            edited_var->type = dropdown_active;
        }
        else
        {
            if (FindVariableByName(&machine, var_name))
            {
                ui_state = CANNOT_CREATE_VAR;

                return;
            }
            else
            {
                AddVariableToMachine(&machine, strdup(var_name), dropdown_active);
            }
        }

        ui_state = EDIT_VARIABLES;
        edited_var = NULL;

        memset(var_name, 0, sizeof(var_name));

        return;
    }

    GuiEnable();

    Rectangle dropdown_rect = {rect.x + 170, rect.y + status_bar_h + 10, 80, 20};

    if (GuiDropdownBox(dropdown_rect, "INTEGER;FLOAT;BOOL", &dropdown_active, dropdown_edit_mode))
        dropdown_edit_mode = !dropdown_edit_mode;
}

static void DoNewConditionGUI(void)
{
    Rectangle rect = {viewport.width / 2 - NEW_CONDITION_WIN_WIDTH / 2,
        viewport.height / 2 + viewport.y - NEW_CONDITION_WIN_HEIGHT / 2,
        NEW_CONDITION_WIN_WIDTH, NEW_CONDITION_WIN_HEIGHT};

    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2 * GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    char title[255] = {0};

    strncpy(title, edited_cond ? "Edit condition" : "New condition", sizeof(title));

    if (GuiWindowBox(rect, title))
    {
        free(vars_str);

        ui_state = EDIT_TRANSITION;
        vars_str = NULL;
        edited_cond = NULL;

        return;
    }

    Rectangle var_dropdown_rect = {rect.x + 10, rect.y + status_bar_h + 10, 210, 20};

    if (GuiDropdownBox(var_dropdown_rect, vars_str, &dropdown_active, dropdown_edit_mode))
        dropdown_edit_mode = !dropdown_edit_mode;

    EditorVariable *selected_var = GetDropdownSelectedVariable();

    if (selected_var)
    {
        Rectangle type_dropdown_rect = {rect.x + 230, rect.y + status_bar_h + 10, 50, 20};

        if (selected_var->type == NBSM_INTEGER || selected_var->type == NBSM_FLOAT)
        {
            if (GuiDropdownBox(type_dropdown_rect, "==;!=;<;<=;>;>=", &dropdown2_active, dropdown2_edit_mode))
                dropdown2_edit_mode = !dropdown2_edit_mode;

            GuiTextBox((Rectangle){rect.x + 290, rect.y + status_bar_h + 10, 80, 20}, val_str, 32, true);
        }
        else
        {
            GuiTextBox(type_dropdown_rect, "==", 32, false);

            input_value.value.b = GuiCheckBox(
                (Rectangle){rect.x + 290, rect.y + status_bar_h + 10, 20, 20},
                "",
                input_value.value.b);
        }

        input_value.type = selected_var->type;

        if (!IsValueInputValid())
            GuiDisable();
    }
    else
    {
        GuiDisable();
    }

    Rectangle btn_rect = {rect.x + rect.width - 60, rect.y + status_bar_h + 10, 50, 20};

    if (GuiButton(btn_rect, edited_cond ? "Update" : "Create"))
    {
        if (input_value.type == NBSM_INTEGER)
            input_value.value.i = atoi(val_str);
        else if (input_value.type == NBSM_FLOAT)
            input_value.value.f = atof(val_str);

        if (edited_cond)
        {
            edited_cond->constant = input_value;
            edited_cond->type = dropdown2_active;
            edited_cond->var = selected_var;
        }
        else
        {
            AddConditionToTransition(selected_trans, dropdown2_active, selected_var, input_value);
        }

        free(vars_str);

        ui_state = EDIT_TRANSITION;
        vars_str = NULL;
        edited_cond = NULL;
    }

    GuiEnable();
}

static void DoEditTransitionGUI(void)
{
    Rectangle rect = {viewport.width / 2 - EDIT_TRANSITION_WIN_WIDTH / 2,
                      viewport.height / 2 + viewport.y - EDIT_TRANSITION_WIN_HEIGHT / 2,
                      EDIT_TRANSITION_WIN_WIDTH, EDIT_TRANSITION_WIN_HEIGHT};

    char title[255] = {0};

    snprintf(title, sizeof(title), "Conditions (%s >> %s)", selected_trans->src_state->name, selected_trans->target_state->name);

    if (GuiWindowBox(rect, title))
    {
        ui_state = DEFAULT_STATE;
        selected_trans = NULL;

        return;
    }

    int item_height = 25;
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);
    unsigned int item_count = selected_trans->condition_count;
    Rectangle scroll_pan_rect = (Rectangle){rect.x, rect.y + (status_bar_h - 2) + 30, rect.width, rect.height - status_bar_h - 30};
    Rectangle content_rect = {0, 0, rect.width - 10, item_count * item_height + 5};
    Rectangle view = GuiScrollPanel(scroll_pan_rect, content_rect, &scroll);

    EditorCondition *cond = selected_trans->conditions;
    unsigned int i = 0;

#ifndef __APPLE__
    BeginScissorMode(view.x, view.y, view.width, view.height);
#endif // __APPLE__

    while (cond)
    {
        EditorCondition *next = cond->next;

        char var_name[255] = {0};

        snprintf(var_name, sizeof(var_name), "%s (%s)", cond->var->name, GetTypeName(cond->var->type));

        GuiTextBox(
            (Rectangle){rect.x + 17, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 210, 20},
            var_name,
            strlen(var_name),
            false);

        const char *type_name = GetConditionTypeName(cond->type);

        GuiTextBox(
            (Rectangle){rect.x + 237, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 20, 20},
            type_name,
            strlen(type_name),
            false);

        GetValueStr(val_str, cond->constant);

        GuiTextBox(
            (Rectangle){rect.x + 267, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 80, 20},
            val_str,
            strlen(val_str),
            false);

        if (GuiButton((Rectangle){rect.x + 357, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 50, 20}, "Edit"))
        {
            edited_cond = cond;
            vars_str = BuildDropdownVariablesString();
            dropdown_active = GetVariableIndex(cond->var);
            dropdown2_active = cond->type;
            ui_state = NEW_CONDITION;
        }

        Rectangle delete_btn_rect = {
            rect.x + rect.width - 65, (scroll_pan_rect.y + i * item_height) + scroll.y + 10, 50, 20};

        if (GuiButton(delete_btn_rect, "Delete"))
            RemoveConditionFromTransition(selected_trans, cond);

        i++;
        cond = next;
    }

#ifndef __APPLE__
    EndScissorMode();
#endif // __APPLE__

    if (GuiButton((Rectangle){rect.x + rect.width - 50, rect.y + status_bar_h + 5, 40, 20}, "Add"))
    {
        memset(val_str, 0, sizeof(val_str));
        memset(&input_value, 0, sizeof(input_value));

        vars_str = BuildDropdownVariablesString();
        dropdown_active = 0;
        dropdown2_active = 0;
        ui_state = NEW_CONDITION;

        return;
    }
}

static void DoNewTransitionGUI(void)
{
    DrawCircleV(transition_start_point, TRANSITION_CIRCLE_RADIUS, TRANSITION_LINE_COLOR);

    Vector2 pos = Vector2Add(GetMousePosition(), (Vector2){CELL_SIZE / 2.0, CELL_SIZE / 2.0});

    pos = SnapPositionToGrid(pos);

    DrawLineEx(transition_start_point, pos, 2.0, TRANSITION_LINE_COLOR);
    DrawCircleV(pos, TRANSITION_CIRCLE_RADIUS, TRANSITION_LINE_COLOR);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        EditorState *state = GetTransitionOveredState(pos);

        if (state)
        {
            EditorTransition *trans = AddTransitionToState(transition_start_state, state);

            trans->render_data.start = transition_start_point;
            trans->render_data.end = pos;
            trans->render_data.is_selected = false;
        }

        ui_state = DEFAULT_STATE;
        transition_start_state = NULL;
    }
}

static void DoDraggingStateGUI(void)
{
    assert(selected_state);

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        Vector2 curr_pos = { selected_state->render_data.rect.x, selected_state->render_data.rect.y };
        Vector2 new_pos = SnapPositionToGrid(Vector2Subtract(GetMousePosition(), drag_offset));

        selected_state->render_data.rect.x = new_pos.x;
        selected_state->render_data.rect.y = new_pos.y;

        UpdateAllStateRelatedTransitionPositions(selected_state, Vector2Subtract(new_pos, curr_pos));
    }
    else
    {
        ui_state = DEFAULT_STATE;
    }
}

static void DoDefaultStateGUI(void)
{
    ResetTransitionSelection(); 

    Vector2 pos = Vector2Add(GetMousePosition(), (Vector2){CELL_SIZE / 2.0, CELL_SIZE / 2.0});

    pos = SnapPositionToGrid(pos);

    EditorState *state = GetTransitionOveredState(pos);

    if (state && !GetSelectedState())
    {
        DrawCircleV(pos, TRANSITION_CIRCLE_RADIUS, TRANSITION_LINE_COLOR);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            transition_start_point = pos;
            transition_start_state = state;
            ui_state = NEW_TRANSITION;

            return;
        }
    }
    else
    {
        EditorTransition *trans = GetSelectedTransition(GetMousePosition());

        if (trans)
            trans->render_data.is_selected = true;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        selected_state = GetSelectedState();

        if (selected_state)
        {
            Vector2 pos = SnapPositionToGrid(GetMousePosition());

            ui_state = DRAGGING_STATE;
            drag_offset = Vector2Subtract(
                pos, (Vector2){selected_state->render_data.rect.x, selected_state->render_data.rect.y});

            return;
        }

        selected_trans = GetSelectedTransition(GetMousePosition());

        if (selected_trans)
        {
            ui_state = EDIT_TRANSITION;
            return;
        }
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        EditorTransition *trans = GetSelectedTransition(GetMousePosition());

        if (trans)
            RemoveTransitionFromState(trans);
    }
}

static void DoCannotRemoveVarGUI(void)
{
    assert(rm_var);

    char text[255] = {0};

    snprintf(text,
             sizeof(text),
             "Impossible to remove '%s' as it's used in at least one condition.",
             rm_var->name);

    DoAlertGUI(text, EDIT_VARIABLES);
}

static void DoCannotCreateVarGUI(void)
{
    char text[255] = {0};

    snprintf(text,
             sizeof(text),
             "Impossible to create a variable '%s' as this name is already used.",
             var_name);

    DoAlertGUI(text, NEW_VARIABLE);
}

static void DoCannotCreateStateGUI(void)
{
    char text[255] = {0};

    snprintf(text,
             sizeof(text),
             "Impossible to create a state '%s' as this name is already used.",
             state_name);

    DoAlertGUI(text, NEW_STATE);
}

static void DoAlertGUI(const char *text, UIState prev_state)
{
    unsigned int win_width = GetTextWidth(text) + 20;

    Rectangle rect = {viewport.width / 2 - win_width / 2,
                      viewport.height / 2 + viewport.y - ALERT_WIN_HEIGHT / 2,
                      win_width, ALERT_WIN_HEIGHT};
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    if (GuiWindowBox(rect, "Alert"))
    {
        ui_state = prev_state;

        return;
    } 

    GuiDrawText(text,
                (Rectangle){rect.x + 10, rect.y + status_bar_h - 15, rect.width, rect.height},
                GUI_TEXT_ALIGN_LEFT,
                ALERT_TEXT_COLOR);
}

static void DoGrid(void)
{
    for (int x = 0; x < viewport.width / CELL_SIZE; x++)
    {
        int pos_x = x * CELL_SIZE;

        DrawLine(pos_x, viewport.y, pos_x, viewport.height, GRID_COLOR);
    }

    for (int y = 0; y < viewport.height / CELL_SIZE; y++)
    {
        int pos_y = y * CELL_SIZE + viewport.y;

        DrawLine(0, pos_y, viewport.width, pos_y, GRID_COLOR);
    }
}

static void DoStatesGUI(void)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorState *next = state->next;

        if (GuiWindowBox(state->render_data.rect, state->name))
        {
            RemoveState(state);
        }
        else
        {
            bool is_initial = GuiCheckBox(
                (Rectangle){state->render_data.rect.x + 10, state->render_data.rect.y + 45, 10, 10},
                "Initial state",
                state->is_initial);

            if (is_initial)
                ToggleInitialState(state);
        }

        state = next;
    }
}

static void DoTransitionsGUI(void)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            Color color = trans->render_data.is_selected ? TRANSITION_SELECTED_LINE_COLOR : TRANSITION_LINE_COLOR;

            DrawCircleV(trans->render_data.start, TRANSITION_CIRCLE_RADIUS, color);
            DrawCircleV(trans->render_data.end, TRANSITION_CIRCLE_RADIUS, color);

            // draw an arrow in the middle of the transition line
            int angle = ((int)Vector2Angle(trans->render_data.start, trans->render_data.end) + 270) % 360;
            Vector2 arrow_pos = GetTransitionArrowPos(trans);

            DrawTexturePro(
                (trans->render_data.is_selected ? selected_arrow_texture : arrow_texture).texture,
                (Rectangle){0, 0, TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE},
                (Rectangle){arrow_pos.x, arrow_pos.y, TRANSITION_ARROW_SIZE, TRANSITION_ARROW_SIZE},
                (Vector2){TRANSITION_ARROW_SIZE / 2, TRANSITION_ARROW_SIZE / 2},
                angle,
                WHITE);
            DrawLineEx(trans->render_data.start, trans->render_data.end, 2.0, color);

            trans = trans->next;
        }

        state = state->next;
    }
}

static void ToggleInitialState(EditorState *initial_state)
{
    EditorState *state = machine.states;

    while (state)
    {
        state->is_initial = false;

        state = state->next;
    }

    initial_state->is_initial = true;
}

static Vector2 SnapPositionToGrid(Vector2 v)
{
    return (Vector2){
        (int)(v.x / CELL_SIZE) * CELL_SIZE,
        max(viewport.y, (int)(v.y / CELL_SIZE) * CELL_SIZE)};
}

static void SetStyle(void)
{
    // GuiSetStyle(DEFAULT, BACKGROUND_COLOR, WINDOW_BACKGROUND_COLOR);
    GuiSetStyle(STATUSBAR, BORDER_WIDTH, 3);
    GuiSetStyle(DEFAULT_STATE, BORDER_WIDTH, 3);
}

static unsigned int ComputeStateWidth(EditorState *state)
{
    return (max(MIN_STATE_WIDTH, GetTextWidth(state->name) + 3 * CELL_SIZE) / CELL_SIZE) * CELL_SIZE;
}

static void RemoveState(EditorState *state)
{
    RemoveAllStateRelatedTransitions(state);
    RemoveStateFromMachine(&machine, state); 
}

static void RemoveAllStateRelatedTransitions(EditorState *state)
{
    EditorState *s = machine.states;

    while (s)
    {
        EditorTransition *trans = s->transitions;

        while (trans)
        {
            EditorTransition *next = trans->next;

            if (trans->src_state == state || trans->target_state == state)
                RemoveTransitionFromState(trans);

            trans = next;
        }

        s = s->next;
    }
}

static void UpdateAllStateRelatedTransitionPositions(EditorState *state, Vector2 move)
{
    if (move.x != 0 || move.y != 0)
    {
        EditorState *s = machine.states;

        while (s)
        {
            EditorTransition *trans = s->transitions;

            while (trans)
            {
                EditorTransition *next = trans->next;

                if (trans->src_state == state)
                {
                    // if it's a state's source transition
                    // update the start point
                    trans->render_data.start = Vector2Add(trans->render_data.start, move);
                }
                else if (trans->target_state == state)
                {
                    // if it's a state's target transition
                    // update the end point
                    trans->render_data.end = Vector2Add(trans->render_data.end, move);
                }

                trans = next;
            }

            s = s->next;
        }
    }
}

static const char *GetTypeName(NBSM_ValueType type)
{
    static const char *names[] = {
        "INTEGER", "FLOAT", "BOOL"
    };

    return names[type];
}

static const char *GetConditionTypeName(NBSM_ConditionType type)
{
    static const char *names[] = { "==", "!=", "<", "<=", ">", ">=" };

    return names[type];
}

static void GetValueStr(char *val_str, NBSM_Value val)
{
    if (val.type == NBSM_BOOLEAN)
        strncpy(val_str, val.value.b ? "True" : "False", 255);
    else if (val.type == NBSM_INTEGER)
        snprintf(val_str, 255, "%d", val.value.i);
    else if (val.type == NBSM_FLOAT)
        snprintf(val_str, 255, "%.2f", val.value.f);
}

static Vector2 GetTransitionArrowPos(EditorTransition *trans)
{
    Vector2 dir = Vector2Normalize(Vector2Subtract(trans->render_data.end, trans->render_data.start));
    float length = Vector2Distance(trans->render_data.start, trans->render_data.end);

    return Vector2Add(trans->render_data.start, Vector2Scale(dir, length / 2));
}

static void ResetTransitionSelection(void)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            trans->render_data.is_selected = false;

            trans = trans->next;
        }

        state = state->next;
    }
}

static char *BuildDropdownVariablesString(void)
{
    // +32 to hold the type name of the variable
    unsigned int length = machine.variable_count * (MAX_VAR_NAME_LENGTH + 32);
    char *str = malloc(length);
    EditorVariable *var = machine.variables;

    memset(str, 0, length);

    while (var)
    {
        char var_name[255] = {0};

        snprintf(var_name, sizeof(var_name), "%s (%s)", var->name, GetTypeName(var->type));
        strncpy(str + strlen(str), var_name, length - strlen(str));

        if (var->next)
            strncpy(str + strlen(str), ";", length - strlen(str));

        var = var->next;
    }

    return str;
}

static EditorVariable *GetDropdownSelectedVariable(void)
{
    EditorVariable *var = machine.variables;
    int i = 0;

    while (var)
    {
        if (i == dropdown_active)
            return var;

        i++;
        var = var->next;
    }

    return NULL;
}

static bool IsValueInputValid(void)
{
    if (input_value.type == NBSM_INTEGER || input_value.type == NBSM_FLOAT)
    {
        int len = strlen(val_str);

        if (len == 0)
            return false;

        for (int i = 0; i < len; i++)
        {
            if (val_str[i] == '-' && i == 0 && len > 1)
                continue;

            if (!(isnumber(val_str[i]) || (input_value.type == NBSM_FLOAT && val_str[i] == '.')))
                return false;
        }
    }

    return true;
}

static bool IsVariableUsedByAnyCondition(EditorVariable *var)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            EditorCondition *cond = trans->conditions;

            while (cond)
            {
                if (cond->var == var)
                    return true;

                cond = cond->next;
            }

            trans = trans->next;
        }

        state = state->next;
    }

    return false;
}

static void UpdateConditionsConstantType(EditorVariable *var, NBSM_ValueType type)
{
    EditorState *state = machine.states;

    while (state)
    {
        EditorTransition *trans = state->transitions;

        while (trans)
        {
            EditorCondition *cond = trans->conditions;

            while (cond)
            {
                if (cond->var == var)
                    cond->constant.type = type;

                cond = cond->next;
            }

            trans = trans->next;
        }

        state = state->next;
    }
}

static int GetVariableIndex(EditorVariable *var)
{
    EditorVariable *v = machine.variables;
    int i = 0;

    while (v)
    {
        if (v == var)
            break;

        i++;
        v = v->next;
    }

    return i;
}

#ifdef EMSCRIPTEN

void DoDownloadGUI(void)
{
    Rectangle rect = {viewport.width / 2 - DOWNLOAD_WIN_WIDTH / 2,
                      viewport.height / 2 + viewport.y - DOWNLOAD_WIN_HEIGHT / 2,
                      DOWNLOAD_WIN_WIDTH, DOWNLOAD_WIN_HEIGHT};
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    if (GuiWindowBox(rect, "Download"))
    {
        ui_state = DEFAULT;
        return;
    }

    GuiDrawText("Choose a file name for your download:",
                (Rectangle){rect.x + 10, rect.y + status_bar_h, rect.width, 20}, GUI_TEXT_ALIGN_LEFT, ALERT_TEXT_COLOR);

    GuiTextBox((Rectangle){rect.x + 10, rect.y + status_bar_h + 25, rect.width - 20, 20}, download_file_name, 255, true);

    if (strlen(download_file_name) == 0)
        GuiDisable();

    if (GuiButton((Rectangle){rect.x + rect.width - 130, rect.y + status_bar_h + 55, 120, 20}, "Download"))
    {
        Download(&machine, download_file_name);

        ui_state = DEFAULT_STATE;
    }

    GuiEnable();
}

#endif // EMSCRIPTEN

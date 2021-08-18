#include <stdio.h>
#include <raylib.h>
#include <raymath.h>

#define RAYGUI_IMPLEMENTATION

#include "raygui.h"

#define NBSM_IMPL
#define NBSM_DESCRIPTOR_API
#define NBSM_Alloc malloc
#define NBSM_Dealloc free

#include "../nbsm.h"

#define SPACE_WIDTH 2048
#define SPACE_HEIGHT 2048
#define VIEWPORT_WIDTH 800
#define VIEWPORT_HEIGHT 600
#define CELL_SIZE 20
#define WINDOW_BACKGROUND_COLOR 0xe8ecf100
#define GRID_COLOR ((Color){191, 191, 191, 255})
#define MIN_STATE_WIDTH (CELL_SIZE*6)
#define NEW_STATE_WIN_WIDTH 300
#define NEW_STATE_WIN_HEIGHT 80
#define VARIABLES_WIN_WIDTH 300
#define VARIABLES_WIN_HEIGHT 200
#define NEW_VARIABLE_WIN_WIDTH 260
#define NEW_VARIABLE_WIN_HEIGHT 100
#define TRANSITION_CIRCLE_RADIUS 3

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

typedef enum
{
    NONE,
    DRAGGING_STATE,
    NEW_STATE,
    NEW_TRANSITION,
    EDIT_VARIABLES,
    NEW_VARIABLE
} State;

typedef struct
{
    Rectangle rect;
} StateUI;

typedef struct
{
    Vector2 start;
    Vector2 end;
} TransitionUI;

typedef struct
{
    int active;
    bool edit_mode;
} VariableUI;

NBSM_MachineDesc machine_desc;

static NBSM_StateDesc *GetSelectedState(void);
static NBSM_StateDesc *GetTransitionOveredState(Vector2 grid_pos);
static void DoGUI(void);
static void DoNewStateWindow(void);
static void DoEditVariablesGUI(void);
static void DoNewVariableGUI(void);
static void DoNewTransitionGUI(void);
static void DoDraggingStateGUI(void);
static void DoDefaultStateGUI(void);
static void DoGrid(void);
static void DoStatesGUI(void);
static void DoTransitionsGUI(void);
static void ToggleInitialState(NBSM_StateDesc *initial_state_desc);
static Vector2 ScreenPositionToSpacePosition(Vector2 v);
static Vector2 SnapPositionToGrid(Vector2 v);
static void SetStyle(void);
static unsigned int ComputeStateWidth(NBSM_StateDesc *state_desc);
static void RemoveState(NBSM_StateDesc *state_desc);
static void RemoveAllStateRelatedTransitions(NBSM_StateDesc *state_desc);
static void UpdateAllStateRelatedTransitionPositions(NBSM_StateDesc *state_desc, Vector2 move);

static Rectangle viewport = {
    0, 0,
    VIEWPORT_WIDTH,
    VIEWPORT_HEIGHT
};

static Rectangle editor_viewport = {
    0, CELL_SIZE, VIEWPORT_WIDTH, VIEWPORT_HEIGHT - CELL_SIZE
};

static State state = NONE;
static NBSM_StateDesc *selected_state = NULL;
static Vector2 drag_offset;
static char state_name[255] = {0};
static char var_name[32] = {0};
static Vector2 transition_start_point;
static NBSM_StateDesc *transition_start_state;
static RenderTexture2D arrow_end_texture;
static Vector2 scroll; // used to store scroll panels scrolling amount
static int dropdown_active = 0;
static bool dropdown_edit_mode = false;

int main(void)
{
    NBSM_InitMachineDesc(&machine_desc);

    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(VIEWPORT_WIDTH, VIEWPORT_HEIGHT, "nbsm editor");

    SetStyle();

    arrow_end_texture = LoadRenderTexture(10, 10);

    BeginTextureMode(arrow_end_texture);
        ClearBackground((Color){255, 255, 255, 0});
        DrawTriangle(
            (Vector2){arrow_end_texture.texture.width / 2, 0},
            (Vector2){0, arrow_end_texture.texture.width},
            (Vector2){arrow_end_texture.texture.width, arrow_end_texture.texture.height},
            RED);
    EndTextureMode();

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(LIGHTGRAY);

            DoGUI();
        EndDrawing();
    }

    UnloadRenderTexture(arrow_end_texture);
    CloseWindow();

    NBSM_DeinitMachineDesc(&machine_desc);

    return 0;
}

static NBSM_StateDesc *GetSelectedState(void)
{
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);
    Vector2 pos = ScreenPositionToSpacePosition(GetMousePosition());

    NBSM_StateDesc *state_desc = machine_desc.states;

    while (state_desc)
    {
        StateUI *render_data = state_desc->user_data;

        // only consider selection when clicking in the status bar and not on the close button

        Rectangle rect = {render_data->rect.x + 10, render_data->rect.y + 10, render_data->rect.width - 30, status_bar_h };

        if (CheckCollisionPointRec(pos, rect))
            return state_desc;

        state_desc = state_desc->next;
    }

    return NULL;
}

static NBSM_StateDesc *GetTransitionOveredState(Vector2 grid_pos)
{
    NBSM_StateDesc *state_desc = machine_desc.states;

    while (state_desc)
    {
        if (!transition_start_state || state_desc != transition_start_state)
        {
            StateUI *render_data = state_desc->user_data;
            Rectangle rect = render_data->rect;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y, rect.width, 1}))
                return state_desc;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y + rect.height, rect.width, 1}))
                return state_desc;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x, rect.y, 1, rect.height}))
                return state_desc;

            if (CheckCollisionPointRec(grid_pos, (Rectangle){rect.x + rect.width, rect.y, 1, rect.height}))
                return state_desc;
        }

        state_desc = state_desc->next;
    }

    return NULL;
}

static void DoGUI(void)
{
    GuiWindowBox(editor_viewport, "Editor"); 
    DoGrid();
    DoStatesGUI(); 
    DoTransitionsGUI();

    if (state == NEW_STATE)
        DoNewStateWindow(); 
    else if (state == NEW_TRANSITION)
        DoNewTransitionGUI();
    else if (state == DRAGGING_STATE)
        DoDraggingStateGUI();
    else if (state == EDIT_VARIABLES)
        DoEditVariablesGUI();
    else if (state == NEW_VARIABLE)
        DoNewVariableGUI();
    else if (state == NONE)
        DoDefaultStateGUI();
}

static void DoNewStateWindow(void)
{
    Rectangle rect = {editor_viewport.width / 2 - NEW_STATE_WIN_WIDTH / 2,
                      editor_viewport.height / 2 + editor_viewport.y - NEW_STATE_WIN_HEIGHT / 2,
                      NEW_STATE_WIN_WIDTH, NEW_STATE_WIN_HEIGHT};
    
    if (GuiWindowBox(rect, "New state"))
    {
        memset(state_name, 0, sizeof(state_name));
        state = NONE;

        return;
    }

    GuiTextBox((Rectangle){rect.x + 10, rect.y + 40, rect.width - 80, 20}, state_name, 255, true);

    if (strlen(state_name) < 1)
        GuiDisable();

    if (GuiButton((Rectangle){rect.x + rect.width - 60, rect.y + 40, 50, 20}, "Create"))
    {
        StateUI *render_data = malloc(sizeof(StateUI));
        NBSM_StateDesc *state_desc = NBSM_AddStateDesc(&machine_desc, strdup(state_name), render_data);
        unsigned int width = ComputeStateWidth(state_desc);
        unsigned int height = CELL_SIZE * 4;
        Vector2 pos = { editor_viewport.width / 2 - width / 2, editor_viewport.height / 2 - height / 2 };

        render_data->rect = (Rectangle){
            ((int)pos.x / CELL_SIZE) * CELL_SIZE,
            ((int)pos.y / CELL_SIZE) * CELL_SIZE,
            width,
            height};

        memset(state_name, 0, sizeof(state_name));
        state = NONE;
    }

    GuiEnable();
}

static void DoEditVariablesGUI(void)
{
    Rectangle rect = {editor_viewport.width / 2 - VARIABLES_WIN_WIDTH / 2,
                      editor_viewport.height / 2 + editor_viewport.y - VARIABLES_WIN_HEIGHT / 2,
                      VARIABLES_WIN_WIDTH, VARIABLES_WIN_HEIGHT};

    if (GuiWindowBox(rect, "Variables"))
    {
        state = NONE;

        return;
    }

    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2 * GuiGetStyle(STATUSBAR, BORDER_WIDTH);
    int item_height = 25;
    unsigned int item_count = machine_desc.value_count;
    Rectangle scroll_pan_rect = (Rectangle){rect.x, rect.y + (status_bar_h - 2) + 30, rect.width, rect.height - status_bar_h - 30};
    Rectangle content_rect = {0, 0, rect.width - 20, item_count * item_height};
    Rectangle view = GuiScrollPanel(scroll_pan_rect, content_rect, &scroll); 

    if (GuiButton((Rectangle){ rect.x + rect.width - 45, rect.y + status_bar_h + 5, 40, 20 }, "Add"))
    {
       state = NEW_VARIABLE;

       return;
    }

    // BeginScissorMode(view.x, view.y, view.width, view.height);

    NBSM_ValueDesc *val_desc = machine_desc.values;
    unsigned int i = 0;

    while (val_desc)
    {
        VariableUI *render_data = val_desc->user_data;

        GuiTextBox(
            (Rectangle){rect.x + 17, (scroll_pan_rect.y + i * item_height) + scroll.y, 150, 20},
            val_desc->name,
            32,
            false); 

        i++;
        val_desc = val_desc->next;
    }

    // EndScissorMode();
}

static void DoNewVariableGUI(void)
{
    Rectangle rect = {editor_viewport.width / 2 - NEW_VARIABLE_WIN_WIDTH / 2,
                      editor_viewport.height / 2 + editor_viewport.y - NEW_VARIABLE_WIN_HEIGHT / 2,
                      NEW_VARIABLE_WIN_WIDTH, NEW_VARIABLE_WIN_HEIGHT};

    if (GuiWindowBox(rect, "New variable")) {
        state = EDIT_VARIABLES;

        return;
    }

    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2 * GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    GuiTextBox(
        (Rectangle){rect.x + 10, rect.y + status_bar_h + 10, 150, 20},
        var_name,
        sizeof(var_name),
        true); 

    if (strlen(var_name) < 1);
        GuiDisable();

    if (GuiButton((Rectangle){rect.x + rect.width - 90, rect.y + rect.height - 30, 80, 20}, "Create")) {
        // char *name = malloc(32);

        // memset(name, 0, 32);

        // VariableUI *render_data = NBSM_Alloc(sizeof(VariableUI));

        // render_data->active = 0;
        // render_data->edit_mode = false; 

        // NBSM_AddValueDesc(&machine_desc, name, NBSM_INTEGER, render_data);

        /*if (content_rect.height > scroll_pan_rect.height)
        scroll.y = -scroll_pan_rect.height;
    */
    }

    GuiEnable();

    Rectangle dropdown_rect = {rect.x + 170, rect.y + status_bar_h + 10, 80, 20};

    if (GuiDropdownBox(dropdown_rect, "INTEGER;FLOAT;BOOL", &dropdown_active, dropdown_edit_mode))
        dropdown_edit_mode = !dropdown_edit_mode;
}

static void DoNewTransitionGUI(void)
{
    DrawCircleV(transition_start_point, TRANSITION_CIRCLE_RADIUS, RED);

    Vector2 pos = Vector2Add(GetMousePosition(), (Vector2){CELL_SIZE / 2.0, CELL_SIZE / 2.0});

    pos = SnapPositionToGrid(ScreenPositionToSpacePosition(pos));

    DrawLineV(transition_start_point, pos, RED);
    DrawCircleV(pos, TRANSITION_CIRCLE_RADIUS, RED);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        NBSM_StateDesc *state_desc = GetTransitionOveredState(pos);

        if (state_desc)
        {
            TransitionUI *render_data = malloc(sizeof(TransitionUI));

            render_data->start = transition_start_point;
            render_data->end = pos;

            NBSM_AddTransitionDesc(transition_start_state, state_desc, render_data);
        }

        state = NONE;
        transition_start_state = NULL;
    }
}

static void DoDraggingStateGUI(void)
{
    assert(selected_state);

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        StateUI *render_data = selected_state->user_data;
        Vector2 curr_pos = { render_data->rect.x, render_data->rect.y };
        Vector2 new_pos = SnapPositionToGrid(Vector2Subtract(ScreenPositionToSpacePosition(GetMousePosition()), drag_offset));

        render_data->rect.x = new_pos.x;
        render_data->rect.y = new_pos.y;

        UpdateAllStateRelatedTransitionPositions(selected_state, Vector2Subtract(new_pos, curr_pos));
    }
    else
    {
        state = NONE;
    }
}

static void DoDefaultStateGUI(void)
{
    if (GuiButton((Rectangle){editor_viewport.width - 110, editor_viewport.y + 35, 100, 20}, "New state"))
    {
        state = NEW_STATE;

        return;
    }

    if (GuiButton((Rectangle){editor_viewport.width - 220, editor_viewport.y + 35, 100, 20}, "Variables"))
    {
        state = EDIT_VARIABLES;

        return;
    }

    Vector2 pos = Vector2Add(GetMousePosition(), (Vector2){CELL_SIZE / 2.0, CELL_SIZE / 2.0});

    pos = SnapPositionToGrid(ScreenPositionToSpacePosition(pos));

    NBSM_StateDesc *state_desc = GetTransitionOveredState(pos);

    if (state_desc)
    {
        DrawCircleV(pos, TRANSITION_CIRCLE_RADIUS, RED);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            transition_start_point = pos;
            transition_start_state = state_desc;
            state = NEW_TRANSITION;

            return;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        selected_state = GetSelectedState();

        if (selected_state)
        {
            Vector2 pos = SnapPositionToGrid(ScreenPositionToSpacePosition(GetMousePosition()));
            Rectangle rect = ((StateUI *)selected_state->user_data)->rect;

            state = DRAGGING_STATE;
            drag_offset = Vector2Subtract(pos, (Vector2){rect.x, rect.y});

            return;
        }
    }
}

static void DoGrid(void)
{
    int status_bar_h = WINDOW_STATUSBAR_HEIGHT + 2*GuiGetStyle(STATUSBAR, BORDER_WIDTH);

    status_bar_h = 40; // TODO: compute

    for (int x = 0; x < editor_viewport.width / CELL_SIZE; x++)
    {
        int pos_x = x * CELL_SIZE;

        DrawLine(pos_x, editor_viewport.y + status_bar_h, pos_x, editor_viewport.height + status_bar_h, GRID_COLOR);
    }

    for (int y = 0; y < editor_viewport.height / CELL_SIZE; y++)
    {
        int pos_y = y * CELL_SIZE + status_bar_h + editor_viewport.y;

        DrawLine(0, pos_y, editor_viewport.width, pos_y, GRID_COLOR);
    }
}

static void DoStatesGUI(void)
{
    NBSM_StateDesc *state_desc = machine_desc.states;

    while (state_desc)
    {
        NBSM_StateDesc *next = state_desc->next;
        StateUI *render_data = state_desc->user_data;

        if (GuiWindowBox(render_data->rect, state_desc->name))
        {
            RemoveState(NBSM_RemoveStateDesc(&machine_desc, state_desc));
        }
        else
        {
            bool is_initial = GuiCheckBox(
                (Rectangle){render_data->rect.x + 10, render_data->rect.y + 45, 10, 10},
                "Initial state",
                state_desc->is_initial);

            if (is_initial)
                ToggleInitialState(state_desc);
        }

        state_desc = next;
    }
}

static void DoTransitionsGUI(void)
{
    NBSM_StateDesc *state_desc = machine_desc.states;

    while (state_desc)
    {
        NBSM_TransitionDesc *trans_desc = state_desc->transitions;

        while (trans_desc)
        {
            TransitionUI *render_data = trans_desc->user_data;
            int angle = ((int)Vector2Angle(render_data->start, render_data->end) + 270) % 360;

            DrawCircleV(render_data->start, TRANSITION_CIRCLE_RADIUS, RED);
            DrawTexturePro(
                arrow_end_texture.texture,
                (Rectangle){0, 0, arrow_end_texture.texture.width, arrow_end_texture.texture.height},
                (Rectangle){render_data->end.x, render_data->end.y, arrow_end_texture.texture.width, arrow_end_texture.texture.height},
                (Vector2){arrow_end_texture.texture.width / 2, arrow_end_texture.texture.height / 2},
                angle,
                WHITE);
            DrawLineV(render_data->start, render_data->end, RED);

            trans_desc = trans_desc->next;
        }

        state_desc = state_desc->next;
    }
}

static void ToggleInitialState(NBSM_StateDesc *initial_state_desc)
{
    NBSM_StateDesc *state_desc = machine_desc.states;

    while (state_desc)
    {
        state_desc->is_initial = false;

        state_desc = state_desc->next;
    }

    initial_state_desc->is_initial = true;
}

static Vector2 ScreenPositionToSpacePosition(Vector2 v)
{
    return Vector2Add(v, (Vector2){viewport.x, viewport.y});
}

static Vector2 SnapPositionToGrid(Vector2 v)
{
    return (Vector2){
        (int)(v.x / CELL_SIZE) * CELL_SIZE,
        (int)(v.y / CELL_SIZE) * CELL_SIZE};
}

static void SetStyle(void)
{
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, WINDOW_BACKGROUND_COLOR);
    GuiSetStyle(STATUSBAR, BORDER_WIDTH, 3);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 3);
}

static unsigned int ComputeStateWidth(NBSM_StateDesc *state_desc)
{
    return (max(MIN_STATE_WIDTH, GetTextWidth(state_desc->name) + 3 * CELL_SIZE) / CELL_SIZE) * CELL_SIZE;
}

static void RemoveState(NBSM_StateDesc *state_desc)
{
    RemoveAllStateRelatedTransitions(state_desc);

    NBSM_StateDesc *rm_state_desc = NBSM_RemoveStateDesc(&machine_desc, state_desc);

    NBSM_Dealloc(rm_state_desc->name);
    NBSM_Dealloc(rm_state_desc->user_data);
    NBSM_Dealloc(rm_state_desc);
}

static void RemoveAllStateRelatedTransitions(NBSM_StateDesc *state_desc)
{
    NBSM_StateDesc *s = machine_desc.states;

    while (s)
    {
        NBSM_TransitionDesc *trans_desc = s->transitions;

        while (trans_desc)
        {
            NBSM_TransitionDesc *next = trans_desc->next;

            if (trans_desc->src_state == state_desc || trans_desc->target_state == state_desc)
            {
                NBSM_TransitionDesc *rm_trans_desc = NBSM_RemoveTransitionDesc(trans_desc);

                NBSM_Dealloc(rm_trans_desc->user_data);
            }

            trans_desc = next;
        }

        s = s->next;
    }
}

static void UpdateAllStateRelatedTransitionPositions(NBSM_StateDesc *state_desc, Vector2 move)
{
    if (move.x != 0 || move.y != 0)
    {
        NBSM_StateDesc *s = machine_desc.states;

        while (s)
        {
            NBSM_TransitionDesc *trans_desc = s->transitions;

            while (trans_desc)
            {
                NBSM_TransitionDesc *next = trans_desc->next;

                if (trans_desc->src_state == state_desc)
                {
                    // if it's a state's source transition
                    // update the start point
                    TransitionUI *render_data = trans_desc->user_data;

                    render_data->start = Vector2Add(render_data->start, move);
                }
                else if (trans_desc->target_state == state_desc)
                {
                    // if it's a state's target transition
                    // update the end point
                    TransitionUI *render_data = trans_desc->user_data;

                    render_data->end = Vector2Add(render_data->end, move);
                }

                trans_desc = next;
            }

            s = s->next;
    }
    }
}
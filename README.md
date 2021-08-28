# nbsm

*nbsm* is a tiny single header C99 library for building finite state machines. It comes with a built-in JSON support to load state
machines from JSON files generated with the [nbsm editor](). You can see a video of the editor [here](https://www.youtube.com/watch?v=f7_d8UYtxwI&ab_channel=NathanBIAGINI).

![nbsm_editor](https://github.com/nathhB/nbsm/blob/main/editor_screenshot.png)

*Disclaimer* : the nbsm editor is a WIP.

## How to use

You can look at the [test suite](https://github.com/nathhB/nbsm/blob/main/tests/suite.c) for examples of using the *nbsm*'s API, but here is a quick overview.

### Creating state machines

```
NBSM_Machine *m = NBSM_Create();
```

### Creating states

```
NBSM_AddState(m, "foo", true); // this is the initial state
NBSM_AddState(m, "bar", false);
NBSM_AddState(m, "plop", false);
```

### Accessing the current state

`m->current_state; // current state machine's state`

### Creating transitions between states

```
NBSM_AddTransition(m, "foo", "bar"); // create a transition between the "foo" state and the "bar" state
NBSM_AddTransition(m, "bar", "plop"); // create a transition between the "bar" state and the "plop" state
```

### Creating variables

Variables can be used to define conditions for transitions. The variable type can be either `integer`, `float` or `boolean`.

```
NBSM_Value *v1 = NBSM_AddInteger(m, "v1");
NBSM_Value *v2 = NBSM_AddFloat(m, "v2");
NBSM_Value *v3 = NBSM_AddBoolean(m, "v3");
```

`integer` and `float` variables are initialized to zero; `boolean` variables are initialized to `false`.

Variables can be updated as follow:

```
NBSM_SetInteger(v1, 42);
NBSM_SetFloat(v2, 42.5f);
NBSM_SetBoolean(v3, true);
```

### Adding conditions to a transition

```
NBSM_Transition *t1 = NBSM_AddTransition(m, "foo", "bar");

NBSM_AddCondition(m, t1, "v1", NBSM_EQ, NBSM_CONST_I(42)); // variable "v1" equals 42 (integer constant)
NBSM_AddCondition(m, t1, "v2", NBSM_LT, NBSM_VAR(m, "v4")); // variable "v2" is lower than variable "v4"
```

Here are the supported condition types:

```
typedef enum
{
    NBSM_EQ,    // equal
    NBSM_NEQ,   // not equal
    NBSM_LT,    // lower than
    NBSM_LTE,   // lower or equal than
    NBSM_GT,    // greater than
    NBSM_GTE    // greater or equal than
} NBSM_ConditionType;
```

For a transition to be executed, all of its conditions must be true. If a transition has no condition, it will always be executed.

### Updating

```
NBSM_Update(m);
```

It needs to be called every frame to evaluate the current state's transitions.

### State hooks

There are three types of state hooks:

* `OnEnter` : called when a new state is entered
* `OnExit` : called when the current state is exited
* `OnUpdate` : called when the current state is updated (every time `NBSM_Update` is called)

State hooks are just callbacks; here is the prototype:

`typedef void (*NBSM_StateHookFunc)(NBSM_Machine *machine, void *user_data);`

To set a state's hooks:

```
NBSM_OnStateEnter(m, "foo", OnEnterFunc); // set the "OnEnter" hook of the "foo" state
NBSM_OnStateExit(m, "foo", OnExitFunc); // set the "OnExit" hook of the "foo" state
NBSM_OnStateUpdate(m, "foo", OnExitFunc); // set the "OnUpdate" hook of the "foo" state
```

The `user_data` parameter of the hook callback is set to the state's user data (see below).

### User data

Generic user data can be attached to both the state machine itself or any of the states:

`NBSM_AttachDataToState(m, "foo", some_data_ptr); // assign a generic data to the "foo" state`

`m->user_data = some_data_ptr; // assign a generic data to a state machine`

### Machine builder

A machine builder is used to generate state machines. Machine builders are used as interfaces between external state machine descriptions (such as a JSON file) and actual state machines.

*nbsm* comes with a built-in JSON machine builder.

#### JSON

JSON supports is made possible thanks to the [json.h](https://github.com/sheredom/json.h) library. To use it in your project, you need to grab the `file.h` header, put it next to the `nbsm.h` header, and define the `NBSM_JSON_BUILDER` before including `nbsm.h` (see the [test suite](https://github.com/nathhB/nbsm/blob/main/tests/suite.c) for an example).

```
NBSM_MachineBuilder *builder = NBSM_CreateBuilderFromJSON(json);
NBSM_Machine *m = NBSM_Build(builder);
```

The JSON file format is pretty straightforward, simply looking at the [json file](https://github.com/nathhB/nbsm/blob/main/tests/test.json) from the test suite should give you all the information you need.

### Pooling

If you plan to create many instances of the same state machine, you can use pooling to avoid reallocating memory every time you need to spawn a new state machine. A machine pool is associated with a machine builder to create new state machines when the pool capacity has been reached.

```
NBSM_MachineBuilder *builder = NBSM_CreateBuilderFromJSON(json);
NBSM_MachinePool *pool = NBSM_CreatePool(builder, 2); // pool is created with 2 state machines

NBSM_Machine *m1 = NBSM_GetFromPool(pool);
NBSM_Machine *m2 = NBSM_GetFromPool(pool);
NBSM_Machine *m3 = NBSM_GetFromPool(pool); // the pool size will be increated to 4 (size * 2)

NBSM_Recycle(pool, m1); // recycle a state machine that is no longer needed and put it back into the pool.
```

**IMPORTANT** : variables will not be automatically reinitialized when using pooling; so, don't forget to initialize the state machine's variables after grabbing it from the pool.

### Cleaning up

Call `NBSM_Destroy` to clean up the memory allocated for a state machine.

Call `NBSM_DestroyBuilder` to clean up the memory allocated for a machine builder.

When using pooling, call `NBSM_DestroyPool` to clean up the memory allocated for the whole pool. Do not use `NBSM_Destroy`.

## nbsm editor

### Compiling

The nbsm editor relies on [raylib](https://github.com/raysan5/raylib) and [json-c](https://github.com/json-c/json-c), you need to compile both; then (using CMake):

```
cd editor
mkdir build
cd build
cmake -DRAYLIB_LIBRARY_PATH=<PATH TO RAYLIB LIB FILE> -DRAYLIB_INCLUDE_PATH=<PATH TO RAYLIB INCLUDE DIR> -DJSONC_LIBRARY_PATH=<PATH TO JSONC-C LIB FILE> -DJSONC_INCLUDE_PATH=<PATH TO JSON-C INCLUDE DIR> ..
```

If you want to compile for the web, use `emcmake` (also make sure you compiled raylib and json-c for the web as well).

### Using the editor

Either open the generated HTML file in your browser if you compiled for the web or use the command line:

`./nbsm_editor /path/to/test.json`

it will crate a new JSON file if the specified one does not exist or load it if it does exist.

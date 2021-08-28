# nbsm

*nbsm* is a tiny single header C99 library for bulding finite state machines. It comes with a built-in JSON support to load state
machines from JSON files generated with the [nbsm editor](). You can see a video of the editor [here](https://www.youtube.com/watch?v=f7_d8UYtxwI&ab_channel=NathanBIAGINI).

## How to use

You can look at the [test suite](https://github.com/nathhB/nbsm/blob/main/tests/suite.c) for examples on how to use the *nbsm*'s API but here is a quick overview.

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

### Creating transitions between states

```
NBSM_AddTransition(m, "foo", "bar"); // create a transition between the "foo" state and the "bar" state
NBSM_AddTransition(m, "bar", "plop"); // create a transition between the "bar" state and the "plop" state
```

### Creating variables

Variables can be used to define conditions for transitions. The type of a variable can be either `integer`, `float` or `boolean`.

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

In order for a transition to be executed, all of its conditions must be true. If a transition has no condition, it will always be executed.

### Updating

```
NBSM_Update(m);
```

It needs to be called every frame to evaluate the current state's transitions.

### State hooks

There are three types of state hooks:

* `OnEnter` : called when a new state is entered
* `OnExit` : called when the current state is exited
* `OnUpdate` : callted when the current state is updated (every time `NBSM_Update` is called)

State hooks are just callback calls, here is the callback prototype:

`typedef void (*NBSM_StateHookFunc)(NBSM_Machine *machine, void *user_data);`

Here is how to set a state's hooks:

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

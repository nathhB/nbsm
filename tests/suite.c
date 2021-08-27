#define NBSM_IMPL
#define NBSM_JSON_BUILDER

#include <stdio.h>

#include "CuTest.h"
#include "../nbsm.h"

static char *ReadTestJSON(void)
{
    FILE *f = fopen("test.json", "r");

    fseek(f, 0, SEEK_END);

    long size = ftell(f);

    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);

    fread(content, 1, size, f);
    fclose(f);

    content[size] = 0;

    return content;
}

void TestPooledMachine(CuTest *tc, NBSM_Machine *m)
{
    NBSM_Value *v1 = NBSM_GetVariable(m, "v1");
    NBSM_Value *v2 = NBSM_GetVariable(m, "v2");
    NBSM_Value *v3 = NBSM_GetVariable(m, "v3");
    NBSM_Value *v4 = NBSM_GetVariable(m, "v4");

    CuAssertTrue(tc, v1->type == NBSM_INTEGER);
    CuAssertTrue(tc, v2->type == NBSM_FLOAT);
    CuAssertTrue(tc, v3->type == NBSM_BOOLEAN);
    CuAssertTrue(tc, v4->type == NBSM_FLOAT);

    CuAssertIntEquals(tc, 0, NBSM_GetInteger(v1));
    CuAssertTrue(tc, NBSM_GetFloat(v2) == 0);
    CuAssertTrue(tc, !NBSM_GetBoolean(v3));
    CuAssertTrue(tc, NBSM_GetFloat(v4) == 0);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetInteger(v1, 42);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.f);
    NBSM_SetBoolean(v3, true);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.625f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "plop", m->current->name);

    NBSM_SetInteger(v1, 30);
    NBSM_SetBoolean(v3, false);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetFloat(v2, -12.f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "toto", m->current->name);
}

void TestPooling(CuTest *tc)
{
    char *json = ReadTestJSON();

    free(json);

    NBSM_MachineBuilder *builder = NBSM_CreateBuilderFromJSON(json);
    NBSM_MachinePool *pool = NBSM_CreatePool(builder, 2);

    NBSM_Machine *m1 = NBSM_GetFromPool(pool);
    NBSM_Machine *m2 = NBSM_GetFromPool(pool);

    CuAssertIntEquals(tc, 2, pool->count); // should not increase the pool
    CuAssertTrue(tc, m1 != m2);

    TestPooledMachine(tc, m1);
    TestPooledMachine(tc, m2);

    NBSM_Recycle(pool, m1);

    NBSM_Machine *m3 = NBSM_GetFromPool(pool);

    // should reuse the "m1" state machine and not build a new one
    CuAssertTrue(tc, m3 == m1);
    CuAssertIntEquals(tc, 2, pool->count); // should not increase the pool

    // reset the variables
    NBSM_SetInteger(NBSM_GetVariable(m3, "v1"), 0);
    NBSM_SetFloat(NBSM_GetVariable(m3, "v2"), 0);
    NBSM_SetBoolean(NBSM_GetVariable(m3, "v3"), false);
    NBSM_SetFloat(NBSM_GetVariable(m3, "v4"), 0);

    TestPooledMachine(tc, m3);

    NBSM_Recycle(pool, m2);

    NBSM_Machine *m4 = NBSM_GetFromPool(pool);

    // should reuse the "m2" state machine and not build a new one
    CuAssertTrue(tc, m4 == m2);
    CuAssertIntEquals(tc, 2, pool->count); // should not increase the pool

    // reset the variables
    NBSM_SetInteger(NBSM_GetVariable(m4, "v1"), 0);
    NBSM_SetFloat(NBSM_GetVariable(m4, "v2"), 0);
    NBSM_SetBoolean(NBSM_GetVariable(m4, "v3"), false);
    NBSM_SetFloat(NBSM_GetVariable(m4, "v4"), 0);

    TestPooledMachine(tc, m4);

    NBSM_Machine *m5 = NBSM_GetFromPool(pool);
    NBSM_Machine *m6 = NBSM_GetFromPool(pool);

    CuAssertIntEquals(tc, 4, pool->count); // should increase the pool

    TestPooledMachine(tc, m5);
    TestPooledMachine(tc, m6);

    NBSM_DestroyPool(pool);
    NBSM_DestroyBuilder(builder);
}

void TestLoadJSON(CuTest *tc)
{ 
    char *json = ReadTestJSON();
    NBSM_MachineBuilder *builder = NBSM_CreateBuilderFromJSON(json);

    free(json);

    NBSM_Machine *m = NBSM_Build(builder);

    NBSM_Value *v1 = NBSM_GetVariable(m, "v1");
    NBSM_Value *v2 = NBSM_GetVariable(m, "v2");
    NBSM_Value *v3 = NBSM_GetVariable(m, "v3");
    NBSM_Value *v4 = NBSM_GetVariable(m, "v4");

    CuAssertTrue(tc, v1->type == NBSM_INTEGER);
    CuAssertTrue(tc, v2->type == NBSM_FLOAT);
    CuAssertTrue(tc, v3->type == NBSM_BOOLEAN);
    CuAssertTrue(tc, v4->type == NBSM_FLOAT);

    CuAssertIntEquals(tc, 0, NBSM_GetInteger(v1));
    CuAssertTrue(tc, NBSM_GetFloat(v2) == 0);
    CuAssertTrue(tc, !NBSM_GetBoolean(v3));
    CuAssertTrue(tc, NBSM_GetFloat(v4) == 0);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetInteger(v1, 42);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.f);
    NBSM_SetBoolean(v3, true);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.625f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "plop", m->current->name);

    NBSM_SetInteger(v1, 30);
    NBSM_SetBoolean(v3, false);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetFloat(v2, -12.f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "toto", m->current->name);

    NBSM_Destroy(m);
    NBSM_DestroyBuilder(builder);
}

void TestTransitionConditions(CuTest *tc)
{
    NBSM_Machine *m = NBSM_Create();

    NBSM_Value *v1 = NBSM_AddInteger(m, "v1");
    NBSM_Value *v2 = NBSM_AddFloat(m, "v2");
    NBSM_Value *v3 = NBSM_AddBoolean(m, "v3");
    NBSM_Value *v4 = NBSM_AddFloat(m, "v4");

    NBSM_SetFloat(v4, -10.150f);

    NBSM_AddState(m, "foo", true);
    NBSM_AddState(m, "bar", false);
    NBSM_AddState(m, "plop", false);
    NBSM_AddState(m, "toto", false);

    NBSM_Transition *t1 = NBSM_AddTransition(m, "foo", "bar");
    NBSM_Transition *t2 = NBSM_AddTransition(m, "bar", "plop");
    NBSM_Transition *t3 = NBSM_AddTransition(m, "plop", "foo");
    NBSM_Transition *t4 = NBSM_AddTransition(m, "foo", "toto");

    NBSM_AddCondition(m, t1, "v1", NBSM_EQ, NBSM_CONST_I(42));
    NBSM_AddCondition(m, t2, "v2", NBSM_GT, NBSM_CONST_F(100));
    NBSM_AddCondition(m, t2, "v3", NBSM_EQ, NBSM_TRUE);
    NBSM_AddCondition(m, t3, "v3", NBSM_EQ, NBSM_FALSE);
    NBSM_AddCondition(m, t3, "v1", NBSM_LT, NBSM_CONST_I(42));
    NBSM_AddCondition(m, t4, "v2", NBSM_LT, NBSM_VAR(m, "v4"));

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetInteger(v1, 42);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.f);
    NBSM_SetBoolean(v3, true);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_SetFloat(v2, 100.5f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "plop", m->current->name);

    NBSM_SetInteger(v1, 30);
    NBSM_SetBoolean(v3, false);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_SetFloat(v2, -12.f);
    NBSM_Update(m);

    CuAssertStrEquals(tc, "toto", m->current->name);

    NBSM_Destroy(m);
}

void TestGetAndSetVariables(CuTest *tc)
{
    NBSM_Machine *m = NBSM_Create();

    NBSM_Value *v1 = NBSM_AddInteger(m, "foo");
    NBSM_Value *v2 = NBSM_AddFloat(m, "bar");
    NBSM_Value *v3 = NBSM_AddBoolean(m, "plop");

    CuAssertTrue(tc, v1->type == NBSM_INTEGER);
    CuAssertTrue(tc, v2->type == NBSM_FLOAT);
    CuAssertTrue(tc, v3->type == NBSM_BOOLEAN);

    CuAssertIntEquals(tc, 0, NBSM_GetInteger(v1));
    CuAssertTrue(tc, NBSM_GetFloat(v2) == 0);
    CuAssertTrue(tc, !NBSM_GetBoolean(v3));

    NBSM_SetInteger(v1, 42);
    NBSM_SetFloat(v2, 42.5f);
    NBSM_SetBoolean(v3, true);

    CuAssertIntEquals(tc, 42, NBSM_GetInteger(v1));
    CuAssertTrue(tc, NBSM_GetFloat(v2) == 42.5);
    CuAssertTrue(tc, NBSM_GetBoolean(v3));

    NBSM_Destroy(m);
}

void TestTransitionNoCondition(CuTest *tc)
{
    NBSM_Machine *m = NBSM_Create();

    NBSM_AddState(m, "foo", true);
    NBSM_AddState(m, "bar", false);
    NBSM_AddState(m, "plop", false);

    NBSM_AddTransition(m, "foo", "bar");
    NBSM_AddTransition(m, "bar", "plop");

    CuAssertPtrNotNull(tc, m->current);
    CuAssertStrEquals(tc, "foo", m->current->name);

    NBSM_Update(m);

    CuAssertStrEquals(tc, "bar", m->current->name);

    NBSM_Update(m);

    CuAssertStrEquals(tc, "plop", m->current->name);

    NBSM_Destroy(m);
}

int main(void)
{
    CuString *output = CuStringNew();
    CuSuite *suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, TestTransitionNoCondition);
    SUITE_ADD_TEST(suite, TestGetAndSetVariables);
    SUITE_ADD_TEST(suite, TestTransitionConditions);
    SUITE_ADD_TEST(suite, TestLoadJSON);
    SUITE_ADD_TEST(suite, TestPooling);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);

    printf("%s\n", output->buffer);

    CuStringDelete(output);
    CuSuiteDelete(suite);

    return 0;
}
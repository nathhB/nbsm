#define NBSM_IMPL
#define NBSM_JSON_BUILDER

#include <stdio.h>

#include "CuTest.h"
#include "../nbsm.h"

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

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);

    printf("%s\n", output->buffer);
}
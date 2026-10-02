#ifndef PROCESS_DATA_TEST_TASK_H
#define PROCESS_DATA_TEST_TASK_H
void TestEnterCritical(void);
void TestExitCritical(void);
#define taskENTER_CRITICAL() TestEnterCritical()
#define taskEXIT_CRITICAL() TestExitCritical()
#endif

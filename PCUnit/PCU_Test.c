#include "include/PCU_Test.h"
#include "include/PCU_Libc.h"

static PCU_Test *current_test;
static int repeat_counter;

static PCU_TestFailure *PCU_TestFailure_new(size_t expected, size_t actual, unsigned long type, const char *expr, const char *file, int line, int repeat);
static void PCU_TestFailure_delete(PCU_TestFailure *self);
static void list_push(PCU_TestFailure *list, PCU_TestFailure *node);
static PCU_TestFailure *list_pop(PCU_TestFailure *list);
#define LIST_EMPTY(list)	((list)->next == list)

static void PCU_Test_clear_result(PCU_Test *self)
{
	PCU_MEMSET(&self->result, 0, sizeof(self->result));
}

void PCU_Test_reset(PCU_Test *self)
{
	if (!self->assertion_list.next) {
		self->assertion_list.next = &self->assertion_list;
		self->assertion_list.prev = &self->assertion_list;
	} else {
		PCU_TestFailure *list = &self->assertion_list;
		while (!LIST_EMPTY(list)) {
			PCU_TestFailure *tmp = list_pop(list);
			PCU_TestFailure_delete(tmp);
		}
	}
	PCU_Test_clear_result(self);
}

static int PCU_Test_setup(const PCU_Test *self)
{
	int ret = 0;
	if (self->setup) {
		ret = self->setup();
	}
	return ret;
}

static int PCU_Test_teardown(const PCU_Test *self)
{
	int ret = 0;
	if (self->teardown) {
		ret = self->teardown();
	}
	return ret;
}

void PCU_Test_run(PCU_Test *self)
{
	int repeat;
	current_test = self;

	repeat = (self->repeat > 0) ? self->repeat : 1;
	for (repeat_counter = 0; repeat_counter < repeat; repeat_counter++) {
		int err;
		err = PCU_Test_setup(self);
		if (err) {
			PCU_TestFailure *node;
			self->result.num_errors_setup++;
			node = PCU_TestFailure_new(0, (size_t) err, PCU_TYPE_SETUP, "SETUP FAILED", "", -1, repeat_counter);
			if (node) {
				list_push(&current_test->assertion_list, node);
			}
			continue;
		}
		self->test();
		err = PCU_Test_teardown(self);
		if (err) {
			PCU_TestFailure *node;
			self->result.num_errors_teardown++;
			node = PCU_TestFailure_new(0, (size_t) err, PCU_TYPE_SETUP, "TEARDOWN FAILED", "", -1, repeat_counter);
			if (node) {
				list_push(&current_test->assertion_list, node);
			}
			continue;
		}
	}
}

void PCU_Test_get_result(PCU_Test *self, PCU_TestResult *result)
{
	*result = self->result;
}

int PCU_Test_is_repeated_test(PCU_Test *self)
{
	if (self->repeat > 0) {
		return 1;
	}
	return 0;
}

int PCU_Test_is_failed(PCU_Test *self)
{
	if (self->result.num_asserts_failed > 0 || self->result.num_errors_setup > 0 || self->result.num_errors_teardown > 0) {
		return 1;
	}
	return 0;
}

int PCU_assert_impl(int passed_flag, size_t expected, size_t actual, unsigned long type, const char *expr, const char *file, int line)
{
	PCU_TestFailure *node;
	current_test->result.num_asserts++;
	current_test->result.num_asserts_ran++;

	if (passed_flag) {
		return 1;
	}

	current_test->result.num_asserts_failed++;

	node = PCU_TestFailure_new(expected, actual, type, expr, file, line, repeat_counter);
	if (node) {
		list_push(&current_test->assertion_list, node);
	}
	return 0;
}

void PCU_msg_impl(const char *msg, const char *file, int line)
{
	PCU_TestFailure *node;
	node = PCU_TestFailure_new((size_t) msg, 0, PCU_TYPE_MSG, "PCU_MSG", file, line, repeat_counter);
	if (node) {
		list_push(&current_test->assertion_list, node);
	}
}

int PCU_repeat_counter(void)
{
	return repeat_counter;
}

const char *PCU_test_name(void)
{
	return current_test->name;
}

static int copy_string(char **dst1, char **dst2, const char *src1, const char *src2, unsigned long type)
{
	char *p1;
	char *p2;
	size_t src1_len;
	size_t src2_len;
	size_t len = 0;
	unsigned long t;
	if (!src1 && !src2) {
		*dst1 = 0;
		*dst2 = 0;
		return 1;
	}
	t = PCU_GET_ASSERT_TYPE(type);
	if (t == PCU_TYPE_NSTR) {
		len = PCU_GET_NSTR_LEN(type);
		src1_len = src1 ? len + 1 : 0;
		src2_len = src2 ? len + 1 : 0;
	} else {
		src1_len = src1 ? PCU_STRLEN(src1) + 1 : 0;
		src2_len = src2 ? PCU_STRLEN(src2) + 1 : 0;
	}
	p1 = (char *) PCU_STR_MALLOC(sizeof(char) * (src1_len + src2_len));
	if (!p1) {
		return 0;
	}
	p2 = p1 + src1_len;
	if (t == PCU_TYPE_NSTR) {
		if (src1) {
			PCU_STRNCPY(p1, src1, len);
			p1[len] = '\0';
		}
		if (src2) {
			PCU_STRNCPY(p2, src2, len);
			p2[len] = '\0';
		}
	} else {
		if (src1) {
			PCU_STRCPY(p1, src1);
		}
		if (src2) {
			PCU_STRCPY(p2, src2);
		}
	}
	*dst1 = src1 ? p1 : 0;
	*dst2 = src2 ? p2 : 0;
	return 1;
}

static PCU_TestFailure *PCU_TestFailure_new(size_t expected, size_t actual, unsigned long type, const char *expr, const char *file, int line, int repeat)
{
	unsigned long t;
	PCU_TestFailure *self = (PCU_TestFailure *) PCU_MALLOC(sizeof(PCU_TestFailure));
	if (!self) {
		PCU_PRINTF3("malloc failed: %s(%d): %s\n", file, line, expr);
		return 0;
	}

	self->type = type;
	t = PCU_GET_ASSERT_TYPE(type);
	switch (t) {
	case PCU_TYPE_NONE:
		break;
	case PCU_TYPE_BOOL:
	case PCU_TYPE_NUM:
	case PCU_TYPE_SETUP:
		self->expected.num = expected;
		self->actual.num = actual;
		break;
	case PCU_TYPE_PTR:
		self->expected.ptr = (const void *) expected;
		self->actual.ptr = (const void *) actual;
		break;
	case PCU_TYPE_STR:
	case PCU_TYPE_NSTR:
	case PCU_TYPE_MSG:
	case PCU_TYPE_FAIL:
		if (!copy_string(&self->expected.str, &self->actual.str, (const char *) expected, (const char *) actual, type)) {
			PCU_PRINTF3("string malloc failed: %s(%d): %s\n", file, line, expr);
			PCU_FREE(self);
			return 0;
		}
		break;
	default:
		self->type = PCU_TYPE_NONE;
		break;
	}

	self->expr = expr;
	self->file = file;
	self->line = line;
	self->repeat = repeat;
	self->next = self->prev = 0;
	return self;
}

static void PCU_TestFailure_delete(PCU_TestFailure *self)
{
	if (!self) return;
	if (PCU_GET_ASSERT_TYPE(self->type) == PCU_TYPE_NSTR || PCU_GET_ASSERT_TYPE(self->type) == PCU_TYPE_STR || 
			PCU_GET_ASSERT_TYPE(self->type) == PCU_TYPE_MSG || PCU_GET_ASSERT_TYPE(self->type) == PCU_TYPE_FAIL) {
		if (self->expected.str) {
			PCU_STR_FREE(self->expected.str);
		} else {
			PCU_STR_FREE(self->actual.str);
		}
	}
	PCU_FREE(self);
}

static void list_push(PCU_TestFailure *list, PCU_TestFailure *node)
{
	node->prev = list->prev;
	node->next = list;
	list->prev->next = node;
	list->prev = node;
}

static PCU_TestFailure *list_pop(PCU_TestFailure *list)
{
	PCU_TestFailure *node;
	node = list->next;
	node->prev->next = node->next;
	node->next->prev = node->prev;
	return node;
}


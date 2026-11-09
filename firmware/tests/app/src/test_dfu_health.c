/*
 * Copyright (c) 2026 Het Bhavsar
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/ztest.h>

#include "dfu_health.h"

ZTEST_SUITE(dfu_health, NULL, NULL, NULL, NULL, NULL);

ZTEST(dfu_health, test_confirms_only_when_healthy_after_a_minute)
{
	zassert_equal(dfu_health_decide(false, 59, true), DFU_WAIT);
	zassert_equal(dfu_health_decide(false, 60, true), DFU_CONFIRM);
}

ZTEST(dfu_health, test_unhealthy_image_reverts)
{
	zassert_equal(dfu_health_decide(false, 299, false), DFU_WAIT);
	zassert_equal(dfu_health_decide(false, 300, false), DFU_REVERT);
}

ZTEST(dfu_health, test_confirmed_image_is_left_alone)
{
	zassert_equal(dfu_health_decide(true, 600, false), DFU_WAIT);
	zassert_equal(dfu_health_decide(true, 60, true), DFU_WAIT);
}

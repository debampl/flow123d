/*
 * check_error_test.cpp
 *
 *  Created on: May 24, 2012
 *      Author: jb
 */



#define FEAL_OVERRIDE_ASSERTS

#include <flow_gtest.hh>
#include "system/exceptions.hh"
#include "system/asserts.hh"
#include "system/system.hh"


TEST(CheckError, error_message) {
	unsigned int err_code;

	err_code = 0;
	chkerr( err_code );

	err_code = 1;
	EXPECT_THROW_WHAT( { chkerr( err_code ); }, ExcChkErr, "1" );
}

TEST(CheckError, assert_message) {
	unsigned int err_code;

	err_code = 0;
	chkerr_assert( err_code );

#ifdef FLOW123D_DEBUG_ASSERTS
    err_code = 1;
    EXPECT_THROW_WHAT( { chkerr_assert( err_code ); }, ExcChkErr, "1" );
#endif
}

#ifdef FLOW123D_DEBUG_ASSERTS
TEST(ASSERT_PERMANENTS, assert_ptr) {
    void * test_ptr = nullptr;
    EXPECT_THROW_WHAT( {ASSERT_PERMANENT_PTR(test_ptr).error();}, feal::Exc_assert, "test_ptr");
}
#endif

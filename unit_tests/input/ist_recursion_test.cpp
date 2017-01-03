/**
 * ist_recursion_test.cpp
 */

#define FEAL_OVERRIDE_ASSERTS

#include <flow_gtest.hh>

#include "input/input_type.hh"

namespace IT = Input::Type;

class RecordGeneratorTest {
public:
	static IT::Record &get_root_rec();
	static IT::Abstract &get_abstract();
	static const IT::Record &get_b_rec();
	static const IT::Record &get_c_rec();
	static const IT::Record &get_d_rec();
};

IT::Record &RecordGeneratorTest::get_root_rec() {
    return IT::Record("Root","")
            .declare_key("a_key", RecordGeneratorTest::get_abstract(), "")
            .declare_key("b_key", RecordGeneratorTest::get_b_rec(), "")
			.close();
}

IT::Abstract &RecordGeneratorTest::get_abstract() {
    return IT::Abstract("AbstractWithRecursion","").close();
}

const IT::Record &RecordGeneratorTest::get_b_rec() {
    return IT::Record("B_rec","")
            .declare_key("a_key", RecordGeneratorTest::get_abstract(), "")
            .declare_key("b_val", IT::Integer(),"")
			.close();
}

const IT::Record &RecordGeneratorTest::get_c_rec() {
    return IT::Record("C_rec","")
            .derive_from(RecordGeneratorTest::get_abstract())
            .declare_key("d_key", RecordGeneratorTest::get_d_rec(), "")
            .declare_key("x_val", IT::Integer(),"")
            .declare_key("y_val", IT::Double(),"")
			.close();
}

const IT::Record &RecordGeneratorTest::get_d_rec() {
    return IT::Record("D_rec","")
            .declare_key("a_key", RecordGeneratorTest::get_abstract(), "")
            .declare_key("d_val", IT::Integer(),"")
			.close();
}


TEST(ISTRecursion, record_recursion) {
	EXPECT_EQ( 4, RecordGeneratorTest::get_c_rec().size() ); // touch of record simulates registrar
	IT::Record root = RecordGeneratorTest::get_root_rec();
	EXPECT_ASSERT_DEATH( { root.finish(); }, "AbstractWithRecursion");
}

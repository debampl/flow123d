/*
 * field_speed_test.cpp
 *
 *  Created on: Feb 3, 2013
 *      Author: jb
 */

#define TEST_USE_PETSC
#define FEAL_OVERRIDE_ASSERTS

#include <flow_gtest_mpi.hh>

#include "system/global_defs.h"


#ifdef FLOW123D_RUN_UNIT_BENCHMARKS

#include <mesh_constructor.hh>
#include "fields/field_constant.hh"
#include "fields/field_formula.hh"
#include "fields/field_python.hh"
#include "fields/field_fe.hh"
#include "fields/field.hh"
#include "fields/field_set.hh"
#include "fields/field_values.hh"
#include "input/input_type.hh"
#include "input/accessors.hh"
#include "input/reader_to_storage.hh"

#include "system/sys_profiler.hh"
#include "system/armor.hh"

#include "mesh/mesh.h"
#include "mesh/accessors.hh"
#include "mesh/range_wrapper.hh"
#include "io/msh_gmshreader.h"

#include <iostream>


FLOW123D_FORCE_LINK_IN_PARENT(field_constant)
FLOW123D_FORCE_LINK_IN_PARENT(field_formula)
FLOW123D_FORCE_LINK_IN_PARENT(field_python)
FLOW123D_FORCE_LINK_IN_PARENT(field_fe)

using namespace std;


static const int loop_call_count = 100000;
static const int list_size = 10;


string field_input = R"JSON(
[   
    {
        region="set_1",

        constant_scalar={ TYPE="FieldConstant", value=1.75 },
        constant_vector_fixed={ TYPE="FieldConstant", value=[1.75, 3.75, 5.75] },
       
        formula_const_scalar={ TYPE="FieldFormula", value="1.75" },
        formula_const_vector_fixed={ TYPE="FieldFormula", value=["1.75", "3.75", "5.75"] },

        formula_simple_scalar={ TYPE="FieldFormula", value="x^2" },
        formula_simple_vector_fixed={ TYPE="FieldFormula", value=["x^2", "y^2", "z^2"] },

        formula_full_scalar={ TYPE="FieldFormula", value="x+y+z+x^2+y^2+z^2" },
        formula_full_vector_fixed={ TYPE="FieldFormula", value=["x+y+x^2+y^2+z^2", "y+z+x^2+y^2+z^2", "x+z+x^2+y^2+z^2"] },

        formula_depth_scalar={ TYPE="FieldFormula", value="d", surface_region=".top side" },
        formula_depth_vector_fixed={ TYPE="FieldFormula", value=["d", "d^2", "d^3"], surface_region=".top side" },

        python_scalar={ TYPE="FieldPython", function="func_const", script_string="def func_const(x,y,z): return ( 1.75, )" },
        python_vector_fixed={ TYPE="FieldPython", function="func_const", script_string="def func_const(x,y,z): return ( 1.75, 3.75, 5.75 )" },

        fe_scalar={ TYPE="FieldFE", mesh_data_file="fields/simplest_cube_data.msh", field_name="scalar" },
        fe_vector_fixed={ TYPE="FieldFE", mesh_data_file="fields/simplest_cube_data.msh", field_name="vector_fixed" }
    },
    {
        region="set_2",

        constant_scalar={ TYPE="FieldConstant", value=1.25 },
        constant_vector_fixed={ TYPE="FieldConstant", value=[1.25, 3.25, 5.25] },

        formula_const_scalar={ TYPE="FieldFormula", value="1.25" },
        formula_const_vector_fixed={ TYPE="FieldFormula", value=["1.25", "3.25", "5.25"] },

        formula_simple_scalar={ TYPE="FieldFormula", value="x^3" },
        formula_simple_vector_fixed={ TYPE="FieldFormula", value=["x^3", "y^3", "z^3"] },

        formula_full_scalar={ TYPE="FieldFormula", value="x+y+z+x^3+y^3+z^3" },
        formula_full_vector_fixed={ TYPE="FieldFormula", value=["x+y+x^3+y^3+z^3", "y+z+x^3+y^3+z^3", "x+z+x^3+y^3+z^3"] },

        formula_depth_scalar={ TYPE="FieldFormula", value="d^2", surface_region=".top side" },
        formula_depth_vector_fixed={ TYPE="FieldFormula", value=["d+1", "d^2+1", "d^3+1"], surface_region=".top side" },

        python_scalar={ TYPE="FieldPython", function="func_const", script_string="def func_const(x,y,z): return ( 1.25, )" },
        python_vector_fixed={ TYPE="FieldPython", function="func_const", script_string="def func_const(x,y,z): return ( 1.25, 3.25, 5.25 )" },

        fe_scalar={ TYPE="FieldFE", mesh_data_file="fields/simplest_cube_data.msh", field_name="scalar" },
        fe_vector_fixed={ TYPE="FieldFE", mesh_data_file="fields/simplest_cube_data.msh", field_name="vector_fixed" }
    }
]
)JSON";



template <typename T>
class FieldSpeed : public testing::Test {
public:
	typedef typename Space<3>::Point Point;
	typedef FieldAlgorithmBase<3, T > FieldBaseType;
	typedef typename T::return_type ReturnType;
    typedef ReturnType(FieldSpeed::*FceType)(Point&, ElementAccessor<3>&);

    ReturnType fce1(Point &, ElementAccessor<3> &) {
    	return data1_;
    }

    ReturnType fce2(Point &, ElementAccessor<3> &) {
    	return data2_;
    }

	void SetUp() {
	    Profiler::instance();

	    FilePath::set_io_dirs(".",UNIT_TESTS_SRC_DIR,"",".");

	    mesh_ = mesh_full_constructor("{mesh_file=\"mesh/simplest_cube.msh\"}");
	}

	void TearDown() {
		Profiler::uninitialize();

		//delete mesh_;
	}

	ReturnType call_test() {

		START_TIMER("single_value");
		for (int i=0; i<list_size*loop_call_count; i++)
			for (auto elm : this->mesh_->elements_range()) {
				test_result_sum_ += field_.value( this->point_, elm);
			}
		END_TIMER("single_value");

		START_TIMER("all_values");
		for (int i=0; i<loop_call_count; i++)
			for (int j=0; j<list_size; j++)
				for (auto elm : this->mesh_->elements_range()) {
					test_result_sum_ += field_.value( this->point_list_.template vec<3>(j), elm);
				}
		END_TIMER("all_values");

		START_TIMER("value_list");
		for (int i=0; i<loop_call_count; i++)
			for (auto elm : this->mesh_->elements_range()) {
				field_.value_list( this->point_list_, elm, value_list);
				test_result_sum_ += value_list[0];
			}
		END_TIMER("value_list");
		return test_result_sum_;
	}

	void set_values(std::string point_coords = "1 2 3") {
        n_comp_ = 3;
        component_names_ = { "component_0", "component_1", "component_2" };

        point_ = Point(point_coords);
        for (int i=0; i<list_size; i++)
			point_list_.set(i) = Armor::vec<3>(point_coords);

    	fce_ = new FceType[mesh_->region_db().size()];
    	data_ = new ReturnType[mesh_->region_db().size()];

    	set_data(data1_);

    	vector<unsigned int> regions_1={0,3,5};
    	vector<unsigned int> regions_2={1,2,4,7};

    	RegionSet set_1,set_2;
    	string set_1_name = "set_1";
    	string set_2_name = "set_2";
    	for( auto i_reg : regions_1) {
    		unsigned int reg_id = mesh_->region_db().get_id(i_reg);
    	    set_1.push_back(mesh_->region_db().find_id(reg_id));
    	    data_[i_reg]=data1_;
    	    fce_[i_reg] = (&FieldSpeed::fce1);
    	}
    	for( auto i_reg : regions_2) {
    		unsigned int reg_id = mesh_->region_db().get_id(i_reg);
    		set_2.push_back(mesh_->region_db().find_id(reg_id));
    		data_[i_reg]=data2_;
    		fce_[i_reg] = (&FieldSpeed::fce2);
    	}
    	( const_cast<RegionDB&>(mesh_->region_db()) ).add_set(set_1_name, set_1);
    	( const_cast<RegionDB&>(mesh_->region_db()) ).add_set(set_2_name, set_2);
	}

	void set_data(FieldValue<3>::Scalar::return_type) {
        data1_ = 1.75;
    	data2_ = 1.25;
    	expect_const_val_ = 13.75;
    	expect_formula_simple_val_ = 9;
    	expect_formula_full_val_ = 268;
    	expect_formula_depth_val_ = 9;
    	expect_fe_val_ = 4.5;
    	test_result_sum_ = 0.0;
    	input_type_name_ = "scalar";
    	value_list= std::vector<ReturnType>(list_size);
	}

	void set_data(FieldValue<3>::VectorFixed::return_type) {
		data1_ = arma::vec3("1.75 3.75 5.75");
		data2_ = arma::vec3("1.25 3.25 5.25");
		expect_const_val_ = arma::vec3("13.75 31.75 49.75");
		expect_formula_simple_val_ = arma::vec3("9 52 153");
		expect_formula_full_val_ = arma::vec3("241 259 250");
		expect_formula_depth_val_ = arma::vec3("13 13 13");
		expect_fe_val_ = arma::vec3("9 18 27");
		test_result_sum_ = arma::vec3("0.0 0.0 0.0");
		input_type_name_ = "vector_fixed";
		value_list= std::vector<ReturnType>(list_size);
	}

	void test_result(FieldValue<3>::Scalar::return_type expected, double multiplicator) {
		EXPECT_DOUBLE_EQ( this->test_result_sum_, multiplicator * expected * loop_call_count );
	}

	void test_result(FieldValue<3>::VectorFixed::return_type expected, double multiplicator) {
		for (int i=0; i<3; i++) {
			EXPECT_DOUBLE_EQ( this->test_result_sum_[i], multiplicator * expected[i] * loop_call_count );
		}
	}

	void profiler_output() {
		static ofstream os( FilePath("speed_test_" + this->input_type_name_ + ".log", FilePath::output_file) );
		Profiler::instance()->output(MPI_COMM_WORLD, os);
		os << "" << std::setfill('=') << setw(80) << "" << std::setfill(' ') << endl << endl;
	}

	void read_input(const string &field_name) {
	    field_.name(field_name);
	    field_.description("xyz");
	    field_.units( UnitSI::dimensionless() );
	    set_of_field_ += field_;

	    Input::Type::Array list_type = Input::Type::Array(set_of_field_.make_field_descriptor_type("FieldSpeedTest"));
	    Input::ReaderToStorage reader( field_input, list_type, Input::FileFormat::format_JSON);
	    Input::Array in_list=reader.get_root_interface<Input::Array>();
	    TimeGovernor tg(0.0, 0.5);
	    field_.set_input_list(in_list, tg);

	    field_.set_mesh(*(this->mesh_));
	    field_.set_components(component_names_);
	    set_of_field_.set_time(tg.step(), LimitSide::right);

	}


    FceType *fce_;
    ReturnType *data_;
    ReturnType data1_;
    ReturnType data2_;
    ReturnType test_result_sum_;
    ReturnType expect_const_val_;
    ReturnType expect_formula_simple_val_;
    ReturnType expect_formula_full_val_;
    ReturnType expect_formula_depth_val_;
    ReturnType expect_fe_val_;
    std::vector<ReturnType> value_list;
    FieldSet set_of_field_;
    Field<3, T> field_;
	Mesh *mesh_;
	Point point_;
	Armor::array point_list_ = Armor::array(3, 1, list_size);

	string input_type_name_;
	std::vector< string > component_names_;
	unsigned int n_comp_;

    inline ReturnType value(Point &p, ElementAccessor<3> &elm) {
    	return (this->*fce_[elm.region_idx().idx()])(p,elm);
    }

};


typedef ::testing::Types< FieldValue<3>::Scalar, FieldValue<3>::VectorFixed > TestedTypes;
TYPED_TEST_CASE(FieldSpeed, TestedTypes);

TYPED_TEST(FieldSpeed, array) {
	this->set_values();
    START_TIMER("array");
	START_TIMER("single_value");
	for (int i=0; i<list_size*loop_call_count; i++)
		for (auto elm : this->mesh_->elements_range()) {
			this->test_result_sum_ += this->data_[elm.region_idx().idx()];
		}
	END_TIMER("single_value");
	END_TIMER("array");

	this->test_result(this->expect_const_val_, 10);
	this->profiler_output();
}


TYPED_TEST(FieldSpeed, virtual_function) {
	this->set_values();
	START_TIMER("virtual_function");
	START_TIMER("single_value");
	for (int i=0; i<list_size*loop_call_count; i++)
		for (auto elm : this->mesh_->elements_range()) {
			this->test_result_sum_ += this->value( this->point_, elm);
		}
	END_TIMER("single_value");

	START_TIMER("all_values");
	for (int i=0; i<loop_call_count; i++)
		for (int j=0; j<list_size; j++)
			for (auto elm : this->mesh_->elements_range()) {
				Space<3>::Point p = this->point_list_.template vec<3>(j);
				this->test_result_sum_ += this->value( p, elm);
			}
	END_TIMER("all_values");
	END_TIMER("virtual_function");

	this->test_result(this->expect_const_val_, 20);
	this->profiler_output();
}



TYPED_TEST(FieldSpeed, field_constant) {
	this->set_values();
	string key_name = "constant_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_constant");
	this->call_test();
	END_TIMER("field_constant");

	this->test_result( this->expect_const_val_, 21 );
	this->profiler_output();
}


TYPED_TEST(FieldSpeed, field_formula_const) {
	this->set_values();
	string key_name = "formula_const_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_formula_const");
	this->call_test();
	END_TIMER("field_formula_const");

	this->test_result( this->expect_const_val_, 21 );
	this->profiler_output();
}


TYPED_TEST(FieldSpeed, field_formula_simple) {
	this->set_values();
	string key_name = "formula_simple_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_formula_simple_expr");
	this->call_test();
	END_TIMER("field_formula_simple_expr");

	this->test_result( this->expect_formula_simple_val_, 21 );
	this->profiler_output();
}


TYPED_TEST(FieldSpeed, field_formula_full) {
	this->set_values();
	string key_name = "formula_full_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_formula_full_expr");
	this->call_test();
	END_TIMER("field_formula_full_expr");

	this->test_result( this->expect_formula_full_val_, 21 );
	this->profiler_output();
}


TYPED_TEST(FieldSpeed, field_formula_depth) {
	this->set_values("0 0 0");
	string key_name = "formula_depth_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_formula_depth_expr");
	this->call_test();
	END_TIMER("field_formula_depth_expr");

	this->test_result( this->expect_formula_depth_val_, 21 );
	this->profiler_output();
}


#ifdef FLOW123D_HAVE_PYTHON
TYPED_TEST(FieldSpeed, field_python) {
	this->set_values();
	string key_name = "python_" + this->input_type_name_;
	this->read_input(key_name);

	START_TIMER("field_python");
	this->call_test();
	END_TIMER("field_python");

	this->test_result( this->expect_const_val_, 21 );
	this->profiler_output();
}
#endif // FLOW123D_HAVE_PYTHON

// PE:
// - it takes way too long  (>5 min)

TYPED_TEST(FieldSpeed, field_fe) {
	this->set_values();
	string key_name = "fe_" + this->input_type_name_;
	this->read_input(key_name);

    START_TIMER("field_fe");
	this->call_test();
	END_TIMER("field_fe");

	this->test_result( this->expect_fe_val_, 21 );
	this->profiler_output();
}



/**
 * Speed results:
 * debug (-g -O0 -NODEBUG) (100 M steps):
 * interface: 1747ms
 * direct   :  361ms
 *
 * optimized -O3 (100 M steps):
 * interface: 123ms
 * direct   : 121ms
 */

#define STEPS (100*1000*1000)

TEST(FieldValue_, speed_test_interface) {

   typedef FieldValue_<1,1, double> T;
   double r_val = 0;


   for(int step=0;step < STEPS; step++) {
       T val(r_val);

       for(unsigned int row=0;row< val.n_cols(); ++row)
           for(unsigned int col=0;col< val.n_rows(); ++col)
               val(row,col)+=step;
   }
   cout << r_val << endl;
}

TEST(FieldValue_, speed_test_direct) {

   double val = 0;

   for(int step=0;step < STEPS; step++) {
       val+=step;
   }
   cout << val << endl;
}

#endif // FLOW123D_RUN_UNIT_BENCHMARKS


/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * 
 * @file    field_formula.cc
 * @brief   
 */



#include "fields/field_formula.hh"
#include "fields/field_instances.hh"	// for instantiation macros
#include "fields/surface_depth.hh"
#include "input/input_type.hh"
//#include "include/arena_alloc.hh"       // bparser
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>



/// Implementation.

namespace it = Input::Type;

FLOW123D_FORCE_LINK_IN_CHILD(field_formula)


template <int spacedim, class Value>
const Input::Type::Record & FieldFormula<spacedim, Value>::get_input_type()
{

    return it::Record("FieldFormula", FieldAlgorithmBase<spacedim,Value>::template_name()+" Field given by runtime interpreted formula.")
            .derive_from(FieldAlgorithmBase<spacedim, Value>::get_input_type())
            .copy_keys(FieldAlgorithmBase<spacedim, Value>::get_field_algo_common_keys())
            .declare_key("value", it::String(), it::Default::obligatory(),
                                        "String, array of strings, or matrix of strings with formulas for individual "
                                        "entries of scalar, vector, or tensor value respectively.\n"
                                        "For vector values, you can use just one string to enter homogeneous vector.\n"
                                        "For square (($N\\times N$))-matrix values, you can use:\n\n"
                                        " - array of strings of size (($N$)) to enter diagonal matrix\n"
                                        " - array of strings of size (($\\frac12N(N+1)$)) to enter symmetric matrix (upper triangle, row by row)\n"
                                        " - just one string to enter (spatially variable) multiple of the unit matrix.\n"
                                        "Formula can contain variables ```x,y,z,t,d``` and usual operators and functions." )
			//.declare_key("unit", FieldAlgorithmBase<spacedim, Value>::get_input_type_unit_si(), it::Default::optional(),
			//							"Definition of unit.")
			.declare_key("surface_direction", it::String(), it::Default("\"0 0 1\""),
										"The vector used to project evaluation point onto the surface.")
			.declare_key("surface_region", it::String(), it::Default::optional(),
										"The name of region set considered as the surface. You have to set surface region if you "
										"want to use formula variable ```d```.")
	        .allow_auto_conversion("value")
			.close();
}



template <int spacedim, class Value>
const int FieldFormula<spacedim, Value>::registrar =
		Input::register_class< FieldFormula<spacedim, Value>, unsigned int >("FieldFormula") +
		FieldFormula<spacedim, Value>::get_input_type().size();



template <int spacedim, class Value>
FieldFormula<spacedim, Value>::FieldFormula( unsigned int n_comp)
: FieldAlgorithmBase<spacedim, Value>(n_comp),
  b_parser_( CacheMapElementNumber::get() ),
  arena_alloc_(nullptr)
{
	this->is_constant_in_space_ = false;
}



template <int spacedim, class Value>
void FieldFormula<spacedim, Value>::init_from_input(const Input::Record &rec, const struct FieldAlgoBaseInitData& init_data) {
	this->init_unit_conversion_coefficient(rec, init_data);

	// read formulas form input
    this->formula_ = rec.val<std::string>("value");
    in_rec_ = rec;
}


template <int spacedim, class Value>
bool FieldFormula<spacedim, Value>::set_time(const TimeStep &time) {

    this->time_=time;
	this->is_constant_in_space_ = false;
    return true;

}


template <int spacedim, class Value>
void FieldFormula<spacedim, Value>::set_mesh(const Mesh *mesh) {
    // create SurfaceDepth object if surface region is set
    std::string surface_region;
    if ( in_rec_.opt_val("surface_region", surface_region) ) {
        surface_depth_ = std::make_shared<SurfaceDepth>(mesh, surface_region, in_rec_.val<std::string>("surface_direction"));
    }
}


template <int spacedim, class Value>
void FieldFormula<spacedim, Value>::cache_update(FieldValueCache<typename Value::element_type> &data_cache,
        ElementCacheMap &cache_map, unsigned int region_patch_idx)
{
    unsigned int reg_chunk_begin = cache_map.region_chunk_begin(region_patch_idx);
    unsigned int reg_chunk_end = cache_map.region_chunk_end(region_patch_idx);

    for (unsigned int i=reg_chunk_begin; i<reg_chunk_end; ++i) {
        res_[i] = 0.0;
    }
    for (auto it : eval_field_data_) {
        // Copy data from dependent fields to arena. Temporary solution.
        // TODO hold field data caches in arena, remove this step
        auto value_cache = it.first->value_cache();
        for (unsigned int i=reg_chunk_begin; i<reg_chunk_end; ++i) {
            if (it.first->name() == "X") {
                x_[i] = value_cache->template vec<3>(i)(0);
                y_[i] = value_cache->template vec<3>(i)(1);
                z_[i] = value_cache->template vec<3>(i)(2);
            } else
        	   it.second[i] = value_cache->data_[i];
        }
    }

    // Get vector of subsets as subarray
    uint subsets_begin = reg_chunk_begin / cache_map.simd_size_double;
    uint subsets_end = reg_chunk_end / cache_map.simd_size_double;
    std::vector<uint> subset_vec;
    subset_vec.assign(subsets_ + subsets_begin, subsets_ + subsets_end);

    b_parser_.set_subset(subset_vec);
    b_parser_.run();
    uint vec_size = CacheMapElementNumber::get();
    for(unsigned int row=0; row < this->value_.n_rows(); row++)
        for(unsigned int col=0; col < this->value_.n_cols(); col++) {
            uint comp_shift = (row*this->value_.n_cols()+col) * vec_size;
            for (unsigned int i=reg_chunk_begin; i<reg_chunk_end; ++i) {
                auto cache_val = data_cache.template mat<Value::NRows_, Value::NCols_>(i);
                cache_val(row, col) = this->unit_conversion_coefficient_ * res_[i + comp_shift];
                data_cache.set(i) = cache_val;
            }
        }
}


template <int spacedim, class Value>
inline arma::vec FieldFormula<spacedim, Value>::eval_depth_var(const Point &p)
{
	if (surface_depth_ && has_depth_var_) {
		// add value of depth
		arma::vec p_depth(spacedim+1);
		p_depth.subvec(0,spacedim-1) = p;
		try {
			p_depth(spacedim) = surface_depth_->compute_distance(p);
		} catch (SurfaceDepth::ExcTooLargeSnapDistance &e) {
			e << SurfaceDepth::EI_FieldTime(this->time_.end());
			e << in_rec_.ei_address();
			throw;
		}
		return p_depth;
	} else {
		return p;
	}
}

template <int spacedim, class Value>
std::vector<const FieldCommon * > FieldFormula<spacedim, Value>::set_dependency(FieldSet &field_set) {
    required_fields_.clear(); // returned value

    // set expression and data to BParser
    try {
        b_parser_.parse( formula_ );
    } catch (std::exception const& e) {
        if (typeid(e) == typeid(bparser::Exception))
            THROW( ExcParserError() << EI_BParserMsg(e.what()) << EI_Formula(formula_) << Input::EI_Address( in_rec_.address_string() ) );
        else throw;
    }
    std::vector<std::string> variables = b_parser_.free_symbols();
    std::sort( variables.begin(), variables.end() );
    variables.erase( std::unique( variables.begin(), variables.end() ), variables.end() );

    has_time_=false;
    sum_shape_sizes_=0; // scecifies size of arena
    for (auto var : variables) {
        if (var == "X" || var == "x" || var == "y" || var == "z") {
            required_fields_.push_back( field_set.field("X") );
            sum_shape_sizes_ += spacedim;
        }
        else if (var == "t") has_time_ = true;
        else {
            auto field_ptr = field_set.field(var);
            if (field_ptr != nullptr) required_fields_.push_back( field_ptr );
            else THROW( FieldSet::ExcUnknownField() << FieldCommon::EI_Field(var) << FieldSet::EI_FieldType("formula") << Input::EI_Address( in_rec_.address_string() ) );
            // TODO: Test the exception, report input line of the formula.
            if (field_ptr->value_cache() == nullptr) THROW( ExcNotDoubleField() << EI_Field(var) << Input::EI_Address( in_rec_.address_string() ) );
            // TODO: Test the exception, report input line of the formula.

            sum_shape_sizes_ += field_ptr->n_shape();
            if (var == "d") {
                field_set.set_surface_depth(this->surface_depth_);
            }
        }
    }

    return required_fields_;
}


template <int spacedim, class Value>
void FieldFormula<spacedim, Value>::cache_reinit(FMT_UNUSED const ElementCacheMap &cache_map)
{
	// Can not compile expression in set_time as the necessary cache size is not known there yet.

    if (arena_alloc_!=nullptr) {
        delete arena_alloc_;
    }
    eval_field_data_.clear();
    uint vec_size = CacheMapElementNumber::get();

    // number of subset alignment to block size
    uint n_subsets = vec_size / cache_map.simd_size_double;
    uint res_comp = Value::NRows_ * Value::NCols_;
    uint n_vectors = sum_shape_sizes_ + res_comp; // needs add space of result vector
    arena_alloc_ = new bparser::ArenaAlloc(cache_map.simd_size_double, n_vectors * vec_size * sizeof(double) + n_subsets * sizeof(uint));
    res_ = arena_alloc_->create_array<double>(vec_size * res_comp);
    for (auto field : required_fields_) {
        std::string field_name = field->name();
        eval_field_data_[field] = arena_alloc_->create_array<double>(field->n_shape() * vec_size);
        if (field_name == "X") {
            X_ = eval_field_data_[field] + 0;
            x_ = eval_field_data_[field] + 0;
            y_ = eval_field_data_[field] + vec_size;
            z_ = eval_field_data_[field] + 2*vec_size;
        }
    }
    subsets_ = arena_alloc_->create_array<uint>(n_subsets);

    // set expression and data to BParser
    if (has_time_) {
        b_parser_.set_constant("t",  {}, {this->time_.end()});
    }
    for (auto field : required_fields_) {
        std::string field_name = field->name();
        if (field_name == "X") {
            b_parser_.set_variable("X",  {3}, X_);
            b_parser_.set_variable("x",  {}, x_);
            b_parser_.set_variable("y",  {}, y_);
            b_parser_.set_variable("z",  {}, z_);
        } else {
            std::vector<uint> f_shape = {};
            if (field->n_shape() > 1) f_shape = field->shape_;
            b_parser_.set_variable(field_name, f_shape, eval_field_data_[field]);
        }
    }
    std::vector<uint> shape = {};
    if (Value::NRows_ > 1) shape.push_back(Value::NRows_);
    if (Value::NCols_ > 1) shape.push_back(Value::NCols_);
    b_parser_.set_variable("_result_", shape, res_);
    b_parser_.compile();
    // set subset vector
    for (uint i=0; i<n_subsets; ++i)
        subsets_[i] = i;
}


template <int spacedim, class Value>
FieldFormula<spacedim, Value>::~FieldFormula() {}


// Instantiations of FieldFormula
INSTANCE_ALL(FieldFormula)

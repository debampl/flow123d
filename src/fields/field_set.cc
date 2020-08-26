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
 * @file    field_set.cc
 * @brief   
 */

#include "fields/field_set.hh"
#include "system/sys_profiler.hh"
#include "input/flow_attribute_lib.hh"
#include "fem/mapping_p1.hh"
#include "mesh/ref_element.hh"
#include <boost/algorithm/string/replace.hpp>



FieldSet::FieldSet()
: x_coord_(1,1), y_coord_(1,1), z_coord_(1,1)
{
    // TODO after replace caches with fields call method cache_reallocate directly
    unsigned int cache_size = 1.1 * ElementCacheMap::n_cached_elements;
    x_coord_.reinit(cache_size);
    x_coord_.resize(cache_size);
    y_coord_.reinit(cache_size);
    y_coord_.resize(cache_size);
    z_coord_.reinit(cache_size);
    z_coord_.resize(cache_size);
}

FieldSet &FieldSet::operator +=(FieldCommon &add_field) {
    FieldCommon *found_field = field(add_field.name());
    if (found_field) {
    	OLD_ASSERT(&add_field==found_field, "Another field of the same name exists when adding field: %s\n",
                add_field.name().c_str());
    } else {
        field_list.push_back(&add_field);
    }
    return *this;
}



FieldSet &FieldSet::operator +=(const FieldSet &other) {
    for(auto field_ptr : other.field_list) this->operator +=(*field_ptr);
    return *this;
}



FieldSet FieldSet::subset(std::vector<std::string> names) const {
    FieldSet set;
    for(auto name : names) set += (*this)[name];
    return set;
}



FieldSet FieldSet::subset( FieldFlag::Flags::Mask mask) const {
    FieldSet set;
    for(auto field : field_list)
        if (field->flags().match(mask))  set += *field;
    return set;
}



Input::Type::Record FieldSet::make_field_descriptor_type(const std::string &equation_name) const {
    string rec_name = equation_name + ":Data";
    string desc = FieldCommon::field_descriptor_record_description(rec_name);
    Input::Type::Record rec = Input::Type::Record(rec_name, desc)
    	.copy_keys(FieldCommon::field_descriptor_record(rec_name));

    for(auto field : field_list) {
        if ( field->flags().match(FieldFlag::declare_input) ) {
            string description =  field->description() + " (($[" + field->units().format_latex() + "]$))";

            // Adding units is not so simple.
            // 1) It must be correct for Latex.
            // 2) It should be consistent with rest of documentation.
            // 3) Should be specified for all fields.
            //if (units != "") description+= " [" +field->units() + "]";

            // TODO: temporary solution, see FieldCommon::multifield_

            std::shared_ptr<Input::Type::TypeBase> field_type_ptr;
            if (field->is_multifield()) {
            	field_type_ptr = std::make_shared<Input::Type::Array>(field->get_multifield_input_type());
            } else {
                field_type_ptr = std::make_shared<Input::Type::Instance>(field->get_input_type());
            }
            ASSERT( field->units().is_def() )(field->input_name()).error("units not def.");
            Input::Type::TypeBase::attribute_map key_attributes = Input::Type::TypeBase::attribute_map(
                    { {FlowAttribute::field_unit(), field->units().json() },
                      {FlowAttribute::field_value_shape(), field->get_value_attribute()} }
           		);
            string default_val = field->input_default();
            if (default_val != "") {
            	boost::replace_all(default_val, "\"", "\\\"");
            	key_attributes[FlowAttribute::field_default_value()] = "\"" + default_val + "\"";
            }
            rec.declare_key(field->input_name(), field_type_ptr, Input::Type::Default::optional(), description, key_attributes);
        }

    }
    return rec.close();
}


/*
Input::Type::Selection FieldSet::make_output_field_selection(const string &name, const string &desc)
{
    namespace IT=Input::Type;
    IT::Selection sel(name, desc);
    int i=0;
    // add value for each field excluding boundary fields
    for( auto field : field_list)
    {
        if ( !field->is_bc() && field->flags().match( FieldFlag::allow_output) )
        {
            string desc = "Output of the field " + field->name() + " (($[" + field->units().format_latex()+"]$))";
            if (field->description().length() > 0)
                desc += " (" + field->description() + ").";
            else
                desc += ".";
            DebugOut() << field->get_value_attribute();

            sel.add_value(i, field->name(), desc, { {FlowAttribute::field_value_shape(), field->get_value_attribute()} } );
            i++;
        }
    }

    return sel;
}
*/


void FieldSet::set_field(const std::string &dest_field_name, FieldCommon &source)
{
    auto &field = (*this)[dest_field_name];
    field.copy_from(source);
}



FieldCommon *FieldSet::field(const std::string &field_name) const {
    for(auto field : field_list)
        if (field->name() ==field_name) return field;
    return nullptr;
}



FieldCommon &FieldSet::operator[](const std::string &field_name) const {
    FieldCommon *found_field=field(field_name);
    if (found_field) return *found_field;

    THROW(ExcUnknownField() << FieldCommon::EI_Field(field_name));
    return *field_list[0]; // formal to prevent compiler warning
}


bool FieldSet::set_time(const TimeStep &time, LimitSide limit_side) {
    bool changed_all=false;
    for(auto field : field_list) changed_all = field->set_time(time, limit_side) || changed_all;
    return changed_all;
}



bool FieldSet::changed() const {
    bool changed_all=false;
    for(auto field : field_list) changed_all = changed_all || field->changed();
    return changed_all;
}



bool FieldSet::is_constant(Region reg) const {
    bool const_all=true;
    for(auto field : field_list) const_all = const_all && field->is_constant(reg);
    return const_all;
}


bool FieldSet::is_jump_time() const {
    bool is_jump = false;
    for(auto field : field_list) is_jump = is_jump || field->is_jump_time();
    return is_jump;
}


void FieldSet::update_coords_caches(ElementCacheMap &cache_map) {
    unsigned int n_cached_elements = cache_map.n_elements();
    std::shared_ptr<EvalPoints> eval_points = cache_map.eval_points();

    for (uint i_elm=0; i_elm<n_cached_elements; ++i_elm) {
        ElementAccessor<3> elm = mesh_->element_accessor( cache_map.elm_idx_on_position(i_elm) );
        unsigned int dim = elm.dim();
        for (uint i_point=0; i_point<eval_points->size(dim); ++i_point) {
            int cache_idx = cache_map.element_eval_point(i_elm, i_point); // index in FieldValueCache
            if (cache_idx<0) continue;
            arma::vec3 coords;
            switch (dim) {
            case 0:
                coords = *elm.node(0);
                break;
            case 1:
                coords = MappingP1<1,3>::project_unit_to_real(RefElement<1>::local_to_bary(eval_points->local_point<1>(i_point)),
                        MappingP1<1,3>::element_map(elm));
                break;
            case 2:
                coords = MappingP1<2,3>::project_unit_to_real(RefElement<2>::local_to_bary(eval_points->local_point<2>(i_point)),
                        MappingP1<2,3>::element_map(elm));
                break;
            case 3:
                coords = MappingP1<3,3>::project_unit_to_real(RefElement<3>::local_to_bary(eval_points->local_point<3>(i_point)),
                        MappingP1<3,3>::element_map(elm));
                break;
            default:
            	coords = arma::vec3("0 0 0"); //Should not happen
            }
            Armor::ArmaMat<double, 1, 1> coord_val;
            coord_val(0,0) = coords(0);
            x_coord_.set(cache_idx) = coord_val;
            coord_val(0,0) = coords(1);
            y_coord_.set(cache_idx) = coord_val;
            coord_val(0,0) = coords(2);
            z_coord_.set(cache_idx) = coord_val;
        }
    }
}


std::ostream &operator<<(std::ostream &stream, const FieldSet &set) {
    for(FieldCommon * field : set.field_list) {
        stream << *field
               << std::endl;
    }
    return stream;
}

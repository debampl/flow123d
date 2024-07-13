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
 * @file    field_fe.cc
 * @brief   
 */


#include <limits>

#include "fields/field_fe.hh"
#include "la/vector_mpi.hh"
#include "fields/field_instances.hh"	// for instantiation macros
#include "input/input_type.hh"
#include "fem/fe_p.hh"
#include "fem/fe_system.hh"
#include "fem/dh_cell_accessor.hh"
#include "fem/mapping_p1.hh"
#include "io/reader_cache.hh"
#include "io/msh_gmshreader.h"
#include "mesh/accessors.hh"
#include "mesh/range_wrapper.hh"
#include "mesh/bc_mesh.hh"
#include "quadrature/quadrature_lib.hh"

#include "system/sys_profiler.hh"
#include "tools/unit_converter.hh"
#include "intersection/intersection_aux.hh"
#include "intersection/intersection_local.hh"
#include "intersection/compute_intersection.hh"




/// Implementation.

namespace it = Input::Type;




FLOW123D_FORCE_LINK_IN_CHILD(field_fe)



/************************************************************************************
 * Implementation of FieldFE methods
 */
template <int spacedim, class Value>
const Input::Type::Record & FieldFE<spacedim, Value>::get_input_type()
{
    return it::Record("FieldFE", FieldAlgorithmBase<spacedim,Value>::template_name()+" Field given by finite element approximation.")
        .derive_from(FieldAlgorithmBase<spacedim, Value>::get_input_type())
        .copy_keys(FieldAlgorithmBase<spacedim, Value>::get_field_algo_common_keys())
        .declare_key("mesh_data_file", IT::FileName::input(), IT::Default::obligatory(),
                "GMSH mesh with data. Can be different from actual computational mesh.")
        .declare_key("input_discretization", FieldFE<spacedim, Value>::get_disc_selection_input_type(), IT::Default::optional(),
                "Section where to find the field.\n Some sections are specific to file format: "
        		"point_data/node_data, cell_data/element_data, -/element_node_data, native/-.\n"
        		"If not given by a user, we try to find the field in all sections, but we report an error "
        		"if it is found in more than one section.")
        .declare_key("field_name", IT::String(), IT::Default::obligatory(),
                "The values of the Field are read from the ```$ElementData``` section with field name given by this key.")
        .declare_key("default_value", IT::Double(), IT::Default::optional(),
                "Default value is set on elements which values have not been listed in the mesh data file.")
        .declare_key("time_unit", UnitConverter::get_input_type(), TimeUnitConversion::get_input_default(),
                "Definition of the unit of all times defined in the mesh data file.")
        .declare_key("read_time_shift", TimeGovernor::get_input_time_type(), IT::Default("0.0"),
                "This key allows reading field data from the mesh data file shifted in time. Considering the time 't', field descriptor with time 'T', "
                "time shift 'S', then if 't > T', we read the time frame 't + S'.")
        .declare_key("interpolation", FieldFE<spacedim, Value>::get_interp_selection_input_type(),
        		IT::Default("\"equivalent_mesh\""), "Type of interpolation applied to the input spatial data.\n"
        		"The default value 'equivalent_mesh' assumes the data being constant on elements living on the same mesh "
        		"as the computational mesh, but possibly with different numbering. In the case of the same numbering, "
        		"the user can set 'identical_mesh' to omit algorithm for guessing node and element renumbering. "
        		"Alternatively, in case of different input mesh, several interpolation algorithms are available.")
        .declare_key("is_boundary", IT::Bool(), IT::Default("false"),
                "Distinguishes bulk / boundary FieldFE.")
        .close();
}

template <int spacedim, class Value>
const Input::Type::Selection & FieldFE<spacedim, Value>::get_disc_selection_input_type()
{
	return it::Selection("FE_discretization",
			"Specify the section in mesh input file where field data is listed.\nSome sections are specific to file format.")
		//.add_value(OutputTime::DiscreteSpace::NODE_DATA, "node_data", "point_data (VTK) / node_data (GMSH)")
		.add_value(OutputTime::DiscreteSpace::ELEM_DATA, "element_data", "cell_data (VTK) / element_data (GMSH)")
		//.add_value(OutputTime::DiscreteSpace::CORNER_DATA, "element_node_data", "element_node_data (only for GMSH)")
		.add_value(OutputTime::DiscreteSpace::NATIVE_DATA, "native_data", "native_data (only for VTK)")
		.close();
}

template <int spacedim, class Value>
const Input::Type::Selection & FieldFE<spacedim, Value>::get_interp_selection_input_type()
{
	return it::Selection("interpolation", "Specify interpolation of the input data from its input mesh to the computational mesh.")
		.add_value(DataInterpolation::identic_msh, "identic_mesh", "Topology and indices of nodes and elements of"
				"the input mesh and the computational mesh are identical. "
				"This interpolation is typically used for GMSH input files containing only the field values without "
				"explicit mesh specification.")
		.add_value(DataInterpolation::equivalent_msh, "equivalent_mesh", "Topologies of the input mesh and the computational mesh "
				"are the same, the node and element numbering may differ. "
				"This interpolation can be used also for VTK input data.") // default value
		.add_value(DataInterpolation::gauss_p0, "P0_gauss", "Topologies of the input mesh and the computational mesh may differ. "
				"Constant values on the elements of the computational mesh are evaluated using the Gaussian quadrature of the fixed order 4, "
				"where the quadrature points and their values are found in the input mesh and input data using the BIH tree search."
				)
		.add_value(DataInterpolation::interp_p0, "P0_intersection", "Topologies of the input mesh and the computational mesh may differ. "
				"Can be applied only for boundary fields. For every (boundary) element of the computational mesh the "
				"intersection with the input mesh is computed. Constant values on the elements of the computational mesh "
				"are evaluated as the weighted average of the (constant) values on the intersecting elements of the input mesh.")
		.close();
}

template <int spacedim, class Value>
const int FieldFE<spacedim, Value>::registrar =
		Input::register_class< FieldFE<spacedim, Value>, unsigned int >("FieldFE") +
		FieldFE<spacedim, Value>::get_input_type().size();



template <int spacedim, class Value>
FieldFE<spacedim, Value>::FieldFE( unsigned int n_comp)
: FieldAlgorithmBase<spacedim, Value>(n_comp),
  dh_(nullptr), field_name_(""), discretization_(OutputTime::DiscreteSpace::UNDEFINED),
  boundary_domain_(false), fe_values_(4)
{
	this->is_constant_in_space_ = false;
}


template<int spacedim, class Value>
typename Field<spacedim,Value>::FieldBasePtr FieldFE<spacedim, Value>::NativeFactory::create_field(Input::Record rec, const FieldCommon &field) {
	Input::Array multifield_arr;
	if (rec.opt_val(field.input_name(), multifield_arr))
	{
		unsigned int position = 0;
		auto it = multifield_arr.begin<Input::AbstractRecord>();
		if (multifield_arr.size() > 1)
			while (index_ != position) {
				++it; ++position;
			}

        Input::Record field_rec = (Input::Record)(*it);
        if (field_rec.val<std::string>("TYPE") == "FieldFE") {
            OutputTime::DiscreteSpace discretization;
            if (field_rec.opt_val<OutputTime::DiscreteSpace>("input_discretization", discretization)) {
                if (discretization == OutputTime::DiscreteSpace::NATIVE_DATA) {
                    std::shared_ptr< FieldFE<spacedim, Value> > field_fe = std::make_shared< FieldFE<spacedim, Value> >(field.n_comp());
                    FieldAlgoBaseInitData init_data(field.input_name(), field.n_comp(), field.units(), field.limits(), field.get_flags());
                    field_fe->init_from_input( *it, init_data );
                    field_fe->set_fe_data(conc_dof_handler_, dof_vector_);
                    return field_fe;
                }
            }
        }
	}

	return NULL;
}


template <int spacedim, class Value>
VectorMPI FieldFE<spacedim, Value>::set_fe_data(std::shared_ptr<DOFHandlerMultiDim> dh, VectorMPI dof_values, unsigned int block_index)
{
    dh_ = dh;
    if (dof_values.size()==0) { //create data vector according to dof handler - Warning not tested yet
        data_vec_ = dh_->create_vector();
        data_vec_.zero_entries();
    } else {
        data_vec_ = dof_values;
    }

    if ( block_index == FieldFE<spacedim, Value>::undef_uint ) {
        this->fill_fe_item<0>();
        this->fill_fe_item<1>();
        this->fill_fe_item<2>();
        this->fill_fe_item<3>();
        this->fe_ = dh_->ds()->fe();
    } else {
        this->fill_fe_system_data<0>(block_index);
        this->fill_fe_system_data<1>(block_index);
        this->fill_fe_system_data<2>(block_index);
        this->fill_fe_system_data<3>(block_index);
        this->fe_ = MixedPtr<FiniteElement>(
                std::dynamic_pointer_cast<FESystem<0>>( dh_->ds()->fe()[Dim<0>{}] )->fe()[block_index],
                std::dynamic_pointer_cast<FESystem<1>>( dh_->ds()->fe()[Dim<1>{}] )->fe()[block_index],
                std::dynamic_pointer_cast<FESystem<2>>( dh_->ds()->fe()[Dim<2>{}] )->fe()[block_index],
                std::dynamic_pointer_cast<FESystem<3>>( dh_->ds()->fe()[Dim<3>{}] )->fe()[block_index]
                );
    }

	// set interpolation
	interpolation_ = DataInterpolation::equivalent_msh;

	region_value_err_.resize(dh_->mesh()->region_db().size());

	return data_vec_;
}



template <int spacedim, class Value>
void FieldFE<spacedim, Value>::cache_update(FieldValueCache<typename Value::element_type> &data_cache,
		ElementCacheMap &cache_map, unsigned int region_patch_idx)
{
    auto region_idx = cache_map.region_idx_from_chunk_position(region_patch_idx);
    if ( (region_idx % 2) == this->boundary_domain_ ) {
        // Skip evaluation of boundary fields on bulk regions and vice versa
        return;
    }

    Armor::ArmaMat<typename Value::element_type, Value::NRows_, Value::NCols_> mat_value;

    unsigned int reg_chunk_begin = cache_map.region_chunk_begin(region_patch_idx);
    unsigned int reg_chunk_end = cache_map.region_chunk_end(region_patch_idx);
    unsigned int last_element_idx = -1;
    DHCellAccessor cell = *( dh_->local_range().begin() ); //needs set variable for correct compiling
    LocDofVec loc_dofs;
    unsigned int range_bgn=0, range_end=0;

    // Throws exception if any element value of processed region is NaN
    unsigned int r_idx = cache_map.eval_point_data(reg_chunk_begin).i_reg_;
    if (region_value_err_[r_idx].is_invalid_)
        THROW( ExcUndefElementValue() << EI_Field(field_name_) << EI_File(reader_file_.filename()) );

    for (unsigned int i_data = reg_chunk_begin; i_data < reg_chunk_end; ++i_data) { // i_eval_point_data
        unsigned int elm_idx = cache_map.eval_point_data(i_data).i_element_;
        if (elm_idx != last_element_idx) {
            ElementAccessor<spacedim> elm(dh_->mesh(), elm_idx);
            fe_values_[elm.dim()].reinit( elm );
            cell = dh_->cell_accessor_from_element( elm_idx );
            loc_dofs = cell.get_loc_dof_indices();
            last_element_idx = elm_idx;
            range_bgn = this->fe_item_[elm.dim()].range_begin_;
            range_end = this->fe_item_[elm.dim()].range_end_;
        }

        unsigned int i_ep=cache_map.eval_point_data(i_data).i_eval_point_;
        //DHCellAccessor cache_cell = cache_map(cell);
        mat_value.fill(0.0);
        for (unsigned int i_dof=range_bgn, i_cdof=0; i_dof<range_end; i_dof++, i_cdof++) {
            mat_value += data_vec_.get(loc_dofs[i_dof]) * this->handle_fe_shape(cell.dim(), i_cdof, i_ep);
        }
        data_cache.set(i_data) = mat_value;
    }
}


template <int spacedim, class Value>
void FieldFE<spacedim, Value>::cache_reinit(const ElementCacheMap &cache_map)
{
    std::shared_ptr<EvalPoints> eval_points = cache_map.eval_points();
    std::array<Quadrature, 4> quads{QGauss(0, 1), this->init_quad<1>(eval_points), this->init_quad<2>(eval_points), this->init_quad<3>(eval_points)};
    fe_values_[0].initialize(quads[0], *this->fe_[0_d], update_values);
    fe_values_[1].initialize(quads[1], *this->fe_[1_d], update_values);
    fe_values_[2].initialize(quads[2], *this->fe_[2_d], update_values);
    fe_values_[3].initialize(quads[3], *this->fe_[3_d], update_values);
}


template <int spacedim, class Value>
template <unsigned int dim>
Quadrature FieldFE<spacedim, Value>::init_quad(std::shared_ptr<EvalPoints> eval_points)
{
    Quadrature quad(dim, eval_points->size(dim));
    for (unsigned int k=0; k<eval_points->size(dim); k++)
        quad.set(k) = eval_points->local_point<dim>(k);
    return quad;
}


template <int spacedim, class Value>
void FieldFE<spacedim, Value>::init_from_input(const Input::Record &rec, const struct FieldAlgoBaseInitData& init_data) {
	this->init_unit_conversion_coefficient(rec, init_data);
	this->in_rec_ = rec;
	flags_ = init_data.flags_;


	// read data from input record
    reader_file_ = FilePath( rec.val<FilePath>("mesh_data_file") );
	field_name_ = rec.val<std::string>("field_name");
	this->boundary_domain_ = rec.val<bool>("is_boundary");
	if (! rec.opt_val<OutputTime::DiscreteSpace>("input_discretization", discretization_) ) {
		discretization_ = OutputTime::DiscreteSpace::UNDEFINED;
	}
	if (! rec.opt_val<DataInterpolation>("interpolation", interpolation_) ) {
		interpolation_ = DataInterpolation::equivalent_msh;
	}
    if (! rec.opt_val("default_value", default_value_) ) {
    	default_value_ = numeric_limits<double>::signaling_NaN();
    }
}



template <int spacedim, class Value>
void FieldFE<spacedim, Value>::set_mesh(const Mesh *mesh) {
    // Mesh can be set only for field initialized from input.
    if ( flags_.match(FieldFlag::equation_input) && flags_.match(FieldFlag::declare_input) ) {
        ASSERT(field_name_ != "").error("Uninitialized FieldFE, did you call init_from_input()?\n");
        if (this->interpolation_ == DataInterpolation::identic_msh) {
        	//DebugOut() << "Identic mesh branch\n";
            source_target_mesh_elm_map_ = ReaderCache::identic_mesh_map(reader_file_, const_cast<Mesh *>(mesh));
        } else {
            //auto source_mesh = ReaderCache::get_mesh(reader_file_);
            // TODO: move the call into equivalent_mesh_map, get rd of get_element_ids method.
            source_target_mesh_elm_map_ = ReaderCache::eqivalent_mesh_map(reader_file_, const_cast<Mesh *>(mesh));
            if (this->interpolation_ == DataInterpolation::equivalent_msh) {
                if (source_target_mesh_elm_map_->empty()) { // incompatible meshes
                    this->interpolation_ = DataInterpolation::gauss_p0;
                    WarningOut().fmt("Source mesh of FieldFE '{}' is not compatible with target mesh.\nInterpolation of input data will be changed to 'P0_gauss'.\n",
                            field_name_);
                }
            } else if (this->interpolation_ == DataInterpolation::interp_p0) {
                if (!this->boundary_domain_) {
                    this->interpolation_ = DataInterpolation::gauss_p0;
                    WarningOut().fmt("Interpolation 'P0_intersection' of FieldFE '{}' can't be used on bulk region.\nIt will be changed to 'P0_gauss'.\n",
                            field_name_);
                }
            }
        }
        if (dh_ == nullptr) {
            if (this->boundary_domain_) this->make_dof_handler( mesh->bc_mesh() );
            else this->make_dof_handler( mesh );
        }
        region_value_err_.resize(mesh->region_db().size());
	}
}



template <int spacedim, class Value>
void FieldFE<spacedim, Value>::make_dof_handler(const MeshBase *mesh) {

	// temporary solution - these objects will be set through FieldCommon
	MixedPtr<FiniteElement> fe;
	switch (this->value_.n_rows() * this->value_.n_cols()) { // by number of components
		case 1: { // scalar
			fe = MixedPtr<FE_P_disc>(0);
			break;
		}
		case 3: { // vector
			 MixedPtr<FE_P_disc>   fe_base(0) ;
			fe = mixed_fe_system(fe_base, FEType::FEVector, 3);
			break;
		}
		case 9: { // tensor
		    MixedPtr<FE_P_disc>   fe_base(0) ;
            fe = mixed_fe_system(fe_base, FEType::FETensor, 9);
			break;
		}
		default:
			ASSERT_PERMANENT(false).error("Should not happen!\n");
	}

	std::shared_ptr<DOFHandlerMultiDim> dh_par = std::make_shared<DOFHandlerMultiDim>( const_cast<MeshBase &>(*mesh) );
    std::shared_ptr<DiscreteSpace> ds = std::make_shared<EqualOrderDiscreteSpace>( &const_cast<MeshBase &>(*mesh), fe);
	dh_par->distribute_dofs(ds);
	dh_ = dh_par;

    this->fill_fe_item<0>();
    this->fill_fe_item<1>();
    this->fill_fe_item<2>();
    this->fill_fe_item<3>();
    this->fe_ = dh_->ds()->fe();

    data_vec_ = VectorMPI::sequential( dh_->lsize() ); // allocate data_vec_
}



template <int spacedim, class Value>
bool FieldFE<spacedim, Value>::set_time(const TimeStep &time) {
	// Time can be set only for field initialized from input.
	if ( flags_.match(FieldFlag::equation_input) && flags_.match(FieldFlag::declare_input) ) {
	    ASSERT(field_name_ != "").error("Uninitialized FieldFE, did you call init_from_input()?\n");
		ASSERT_PTR(dh_)(field_name_).error("Null target mesh pointer of finite element field, did you call set_mesh()?\n");
		if ( reader_file_ == FilePath() ) return false;

		unsigned int n_components = this->value_.n_rows() * this->value_.n_cols();
		double time_unit_coef = time.read_coef(in_rec_.find<Input::Record>("time_unit"));
		double time_shift = time.read_time( in_rec_.find<Input::Tuple>("read_time_shift") );
		double read_time = (time.end()+time_shift) / time_unit_coef;

		unsigned int n_entities, bdr_shift;
		bool is_native = (this->discretization_ == OutputTime::DiscreteSpace::NATIVE_DATA);
		if (is_native) {
		    n_components *= dh_->max_elem_dofs();
		}
		if (this->interpolation_==DataInterpolation::identic_msh) {
		    n_entities = source_target_mesh_elm_map_->bulk.size() + source_target_mesh_elm_map_->boundary.size();
		    bdr_shift = source_target_mesh_elm_map_->bulk.size();
		} else {
		    auto reader_mesh = ReaderCache::get_mesh(reader_file_);
		    n_entities = reader_mesh->n_elements() + reader_mesh->bc_mesh()->n_elements();
		    bdr_shift = reader_mesh->n_elements();
		}

        BaseMeshReader::HeaderQuery header_query(field_name_, read_time, this->discretization_, dh_->hash());
        auto reader = ReaderCache::get_reader(reader_file_);
        auto header = reader->find_header(header_query);
        this->input_data_cache_ = reader->template get_element_data<double>(
            header, n_entities, n_components, bdr_shift);

		if (is_native) {
			this->calculate_element_values();
		} else if (this->interpolation_==DataInterpolation::identic_msh) {
			this->calculate_element_values();
		} else if (this->interpolation_==DataInterpolation::equivalent_msh) {
			this->calculate_element_values();
		} else if (this->interpolation_==DataInterpolation::gauss_p0) {
			this->interpolate_gauss();
		} else { // DataInterpolation::interp_p0
			this->interpolate_intersection();
		}

		return true;
	} else return false;

}


template <int spacedim, class Value>
void FieldFE<spacedim, Value>::interpolate_gauss()
{
	static const unsigned int quadrature_order = 4; // parameter of quadrature
	std::shared_ptr<Mesh> source_mesh = ReaderCache::get_mesh(reader_file_);
	std::vector<unsigned int> searched_elements; // stored suspect elements in calculating the intersection
	std::vector<arma::vec::fixed<3>> q_points; // real coordinates of quadrature points
	std::vector<double> q_weights; // weights of quadrature points
	unsigned int quadrature_size=0; // size of quadrature point and weight vector
	std::vector<double> sum_val(dh_->max_elem_dofs()); // sum of value of one quadrature point
	unsigned int elem_count; // count of intersect (source) elements of one quadrature point
	std::vector<double> elem_value(dh_->max_elem_dofs()); // computed value of one (target) element
	bool contains; // sign if source element contains quadrature point

	{
		// set size of vectors to maximal count of quadrature points
		QGauss quad(3, quadrature_order);
		q_points.resize(quad.size());
		q_weights.resize(quad.size());
	}

	for (auto cell : dh_->own_range()) {
		auto ele = cell.elm();
		std::fill(elem_value.begin(), elem_value.end(), 0.0);
		switch (cell.dim()) {
		case 0:
			quadrature_size = 1;
			q_points[0] = *ele.node(0);
			q_weights[0] = 1.0;
			break;
		case 1:
			quadrature_size = compute_fe_quadrature<1>(q_points, q_weights, ele, quadrature_order);
			break;
		case 2:
			quadrature_size = compute_fe_quadrature<2>(q_points, q_weights, ele, quadrature_order);
			break;
		case 3:
			quadrature_size = compute_fe_quadrature<3>(q_points, q_weights, ele, quadrature_order);
			break;
		}
		searched_elements.clear();
		source_mesh->get_bih_tree().find_bounding_box(ele.bounding_box(), searched_elements);

		auto r_idx = cell.elm().region_idx().idx();
		std::string reg_name = cell.elm().region().label();
		for (unsigned int i=0; i<quadrature_size; ++i) {
			std::fill(sum_val.begin(), sum_val.end(), 0.0);
			elem_count = 0;
			for (std::vector<unsigned int>::iterator it = searched_elements.begin(); it!=searched_elements.end(); it++) {
				ElementAccessor<3> elm = source_mesh->element_accessor(*it);
				contains=false;
				switch (elm->dim()) {
				case 0:
					contains = arma::norm(*elm.node(0) - q_points[i], 2) < 4*std::numeric_limits<double>::epsilon();
					break;
				case 1:
					contains = MappingP1<1,3>::contains_point(q_points[i], elm);
					break;
				case 2:
					contains = MappingP1<2,3>::contains_point(q_points[i], elm);
					break;
				case 3:
					contains = MappingP1<3,3>::contains_point(q_points[i], elm);
					break;
				default:
					ASSERT_PERMANENT(false).error("Invalid element dimension!");
				}
				if ( contains ) {
					// projection point in element
					unsigned int index = sum_val.size() * (*it);
					for (unsigned int j=0; j < sum_val.size(); j++) {
						sum_val[j] += get_scaled_value(index+j, dh_->mesh()->elem_index( cell.elm_idx() ), reg_name, region_value_err_[r_idx]);
					}
					++elem_count;
				}
			}

			if (elem_count > 0) {
				for (unsigned int j=0; j < sum_val.size(); j++) {
					elem_value[j] += (sum_val[j] / elem_count) * q_weights[i];
				}
			}
		}

		LocDofVec loc_dofs;
		loc_dofs = cell.get_loc_dof_indices();

		ASSERT_LE(loc_dofs.n_elem, elem_value.size());
		for (unsigned int i=0; i < elem_value.size(); i++) {
			ASSERT_LT( loc_dofs[i], (int)data_vec_.size());
			data_vec_.set( loc_dofs[i], elem_value[i] );
		}
	}
}


template <int spacedim, class Value>
void FieldFE<spacedim, Value>::interpolate_intersection()
{
	std::shared_ptr<Mesh> source_mesh = ReaderCache::get_mesh(reader_file_);
	std::vector<unsigned int> searched_elements; // stored suspect elements in calculating the intersection
	std::vector<double> value(dh_->max_elem_dofs());
	double total_measure;
	double measure = 0;

	for (auto elm : dh_->mesh()->elements_range()) {
		if (elm.dim() == 3) {
			THROW( ExcInvalidElemeDim() << EI_ElemIdx(elm.idx()) );
		}

		double epsilon = 4* numeric_limits<double>::epsilon() * elm.measure();
		auto r_idx = elm.region_idx().idx();
		std::string reg_name = elm.region().label();

		// gets suspect elements
		if (elm.dim() == 0) {
			searched_elements.clear();
			source_mesh->get_bih_tree().find_point(*elm.node(0), searched_elements);
		} else {
			BoundingBox bb = elm.bounding_box();
			searched_elements.clear();
			source_mesh->get_bih_tree().find_bounding_box(bb, searched_elements);
		}

		// set zero values of value object
		std::fill(value.begin(), value.end(), 0.0);
		total_measure=0.0;

		START_TIMER("compute_pressure");
		ADD_CALLS(searched_elements.size());


        for (std::vector<unsigned int>::iterator it = searched_elements.begin(); it!=searched_elements.end(); it++)
        {
            ElementAccessor<3> source_elm = source_mesh->element_accessor(*it);
            if (source_elm->dim() == 3) {
                // get intersection (set measure = 0 if intersection doesn't exist)
                switch (elm.dim()) {
                    case 0: {
                        arma::vec::fixed<3> real_point = *elm.node(0);
                        arma::mat::fixed<3, 4> elm_map = MappingP1<3,3>::element_map(source_elm);
                        arma::vec::fixed<4> unit_point = MappingP1<3,3>::project_real_to_unit(real_point, elm_map);

                        measure = (std::fabs(arma::sum( unit_point )-1) <= 1e-14
                                        && arma::min( unit_point ) >= 0)
                                            ? 1.0 : 0.0;
                        break;
                    }
                    case 1: {
                        IntersectionAux<1,3> is(elm.idx(), source_elm.idx());
                        ComputeIntersection<1,3> CI(elm, source_elm);
                        CI.init();
                        CI.compute(is);

                        IntersectionLocal<1,3> ilc(is);
                        measure = ilc.compute_measure() * elm.measure();
                        break;
                    }
                    case 2: {
                        IntersectionAux<2,3> is(elm.idx(), source_elm.idx());
                        ComputeIntersection<2,3> CI(elm, source_elm);
                        CI.init();
                        CI.compute(is);

                        IntersectionLocal<2,3> ilc(is);
                        measure = 2 * ilc.compute_measure() * elm.measure();
                        break;
                    }
                }

				//adds values to value_ object if intersection exists
				if (measure > epsilon) {
					unsigned int index = value.size() * (*it);
			        for (unsigned int i=0; i < value.size(); i++) {
			        	value[i] += get_scaled_value(index+i, dh_->mesh()->elem_index( elm.idx() ), reg_name, region_value_err_[r_idx]) * measure;
			        }
					total_measure += measure;
				}
			}
		}

		// computes weighted average, store it to data vector
		if (total_measure > epsilon) {
			DHCellAccessor cell = dh_->cell_accessor_from_element(elm.idx());
			LocDofVec loc_dofs = cell.get_loc_dof_indices();

			ASSERT_LE(loc_dofs.n_elem, value.size());
			for (unsigned int i=0; i < value.size(); i++) {
				data_vec_.set(loc_dofs[i], value[i] / total_measure);
			}
		} else {
			WarningOut().fmt("Processed element with idx {} is out of source mesh!\n", elm.idx());
		}
		END_TIMER("compute_pressure");

	}
}


template <int spacedim, class Value>
void FieldFE<spacedim, Value>::calculate_element_values()
{
    // Same algorithm as in output of Node_data. Possibly code reuse.
    unsigned int data_vec_i, vec_inc;
    std::vector<unsigned int> count_vector(data_vec_.size(), 0);
    data_vec_.zero_entries();
    std::vector<LongIdx> &source_target_vec = (dynamic_cast<BCMesh*>(dh_->mesh()) != nullptr) ? source_target_mesh_elm_map_->boundary : source_target_mesh_elm_map_->bulk;

    unsigned int shift;
    if (this->boundary_domain_) {
        if (this->interpolation_==DataInterpolation::identic_msh) shift = source_target_mesh_elm_map_->bulk.size();
	    else shift = ReaderCache::get_mesh(reader_file_)->n_elements();
    } else {
        shift = 0;
    }

    ASSERT_GT(region_value_err_.size(), 0)(field_name_).error("Vector of region isNaN flags is not initialized. Did you call set_mesh or set_fe_data?\n");
    for (auto r : region_value_err_)
        r = RegionValueErr();

    for (auto cell : dh_->own_range()) {
        LocDofVec loc_dofs = cell.get_loc_dof_indices();
        //DebugOut() << cell.elm_idx() << " < " << source_target_vec.size() << "\n";
        int source_idx = source_target_vec[cell.elm_idx()];

        if (source_idx == (int)(Mesh::undef_idx)) {
            data_vec_i = source_idx;
            vec_inc = 0;
        } else {
            data_vec_i = (source_idx + shift) * dh_->max_elem_dofs();
            vec_inc = 1;
        }
        auto r_idx = cell.elm().region_idx().idx();
        std::string reg_name = cell.elm().region().label();
        for (unsigned int i=0; i<loc_dofs.n_elem; ++i) {
            ASSERT_LT(loc_dofs[i], (LongIdx)data_vec_.size());
            data_vec_.add( loc_dofs[i], get_scaled_value(data_vec_i, dh_->mesh()->elem_index( cell.elm_idx() ), reg_name, region_value_err_[r_idx]) );
            ++count_vector[ loc_dofs[i] ];
            data_vec_i += vec_inc;
        }
    }

    // compute averages of values
    for (unsigned int i=0; i<data_vec_.size(); ++i) {
        if (count_vector[i]>0) data_vec_.normalize(i, count_vector[i]);
    }
}


//template <int spacedim, class Value>
//void FieldFE<spacedim, Value>::calculate_native_values(ElementDataCache<double>::CacheData data_cache)
//{
//	// Same algorithm as in output of Node_data. Possibly code reuse.
//	unsigned int data_vec_i;
//	std::vector<unsigned int> count_vector(data_vec_.size(), 0);
//	data_vec_.zero_entries();
//	std::vector<LongIdx> &source_target_vec = (dynamic_cast<BCMesh*>(dh_->mesh()) != nullptr) ? source_target_mesh_elm_map_->boundary : source_target_mesh_elm_map_->bulk;
//
//	// iterate through cells, assembly MPIVector
//	for (auto cell : dh_->own_range()) {
//		LocDofVec loc_dofs = cell.get_loc_dof_indices();
//		data_vec_i = source_target_vec[cell.elm_idx()] * loc_dofs.n_elem;
//		for (unsigned int i=0; i<loc_dofs.n_elem; ++i, ++data_vec_i) {
//		    data_vec_.add( loc_dofs[i], (*data_cache)[ data_vec_i ] );
//		    ++count_vector[ loc_dofs[i] ];
//		}
//	}
//
//	// compute averages of values
//	for (unsigned int i=0; i<data_vec_.size(); ++i) {
//		if (count_vector[i]>0) data_vec_.normalize(i, count_vector[i]);
//	}
//}
//
//
//template <int spacedim, class Value>
//void FieldFE<spacedim, Value>::calculate_equivalent_values(ElementDataCache<double>::CacheData data_cache)
//{
//	// Same algorithm as in output of Node_data. Possibly code reuse.
//	unsigned int data_vec_i;
//	std::vector<unsigned int> count_vector(data_vec_.size(), 0);
//	data_vec_.zero_entries();
//	std::vector<LongIdx> &source_target_vec = (dynamic_cast<BCMesh*>(dh_->mesh()) != nullptr) ? source_target_mesh_elm_map_->boundary : source_target_mesh_elm_map_->bulk;
//	unsigned int shift;
//	if (this->boundary_domain_) {
//		if (this->interpolation_==DataInterpolation::identic_msh) shift = this->comp_mesh_->n_elements();
//		else shift = ReaderCache::get_mesh(reader_file_)->n_elements();
//	} else {
//	    shift = 0;
//	}
//
//	// iterate through elements, assembly global vector and count number of writes
//	for (auto cell : dh_->own_range()) {
//		LocDofVec loc_dofs = cell.get_loc_dof_indices();
//		//DebugOut() << cell.elm_idx() << " < " << source_target_vec.size() << "\n";
//		int source_idx = source_target_vec[cell.elm_idx()];
//		if (source_idx == (int)(Mesh::undef_idx)) { // undefined value in input data mesh
//			if ( std::isnan(default_value_) )
//				THROW( ExcUndefElementValue() << EI_Field(field_name_) );
//			for (unsigned int i=0; i<loc_dofs.n_elem; ++i) {
//				ASSERT_LT(loc_dofs[i], (LongIdx)data_vec_.size());
//				data_vec_.add( loc_dofs[i], default_value_ * this->unit_conversion_coefficient_ );
//				++count_vector[ loc_dofs[i] ];
//			}
//		} else {
//			data_vec_i = (source_idx + shift) * dh_->max_elem_dofs();
//			for (unsigned int i=0; i<loc_dofs.n_elem; ++i, ++data_vec_i) {
//				ASSERT_LT(loc_dofs[i], (LongIdx)data_vec_.size());
//				data_vec_.add( loc_dofs[i], (*data_cache)[data_vec_i] );
//				++count_vector[ loc_dofs[i] ];
//			}
//		}
//	}
//
//	// compute averages of values
//	for (unsigned int i=0; i<data_vec_.size(); ++i) {
//		if (count_vector[i]>0) data_vec_.normalize(i, count_vector[i]);
//	}
//}
//
//
template <int spacedim, class Value>
void FieldFE<spacedim, Value>::native_data_to_cache(ElementDataCache<double> &output_data_cache) {
	//ASSERT_EQ(output_data_cache.n_values() * output_data_cache.n_comp(), dh_->distr()->lsize()).error();
	unsigned int n_vals = output_data_cache.n_comp() * output_data_cache.n_dofs_per_element();
	double loc_values[n_vals];
	unsigned int i;

	for (auto dh_cell : dh_->own_range()) {
		LocDofVec loc_dofs = dh_cell.get_loc_dof_indices();
		for (i=0; i<loc_dofs.n_elem; ++i) loc_values[i] = data_vec_.get( loc_dofs[i] );
		for ( ; i<n_vals; ++i) loc_values[i] = numeric_limits<double>::signaling_NaN();
		output_data_cache.store_value( dh_cell.local_idx(), loc_values );
	}

	output_data_cache.set_dof_handler_hash( dh_->hash() );
}



template <int spacedim, class Value>
inline unsigned int FieldFE<spacedim, Value>::data_size() const {
	return data_vec_.size();
}



template <int spacedim, class Value>
void FieldFE<spacedim, Value>::local_to_ghost_data_scatter_begin() {
	data_vec_.local_to_ghost_begin();
}



template <int spacedim, class Value>
void FieldFE<spacedim, Value>::local_to_ghost_data_scatter_end() {
	data_vec_.local_to_ghost_end();
}



template <int spacedim, class Value>
double FieldFE<spacedim, Value>::get_scaled_value(int i_cache_el, unsigned int elm_idx, const std::string &region_name,
        RegionValueErr &actual_compute_region_error) {
    double return_val;
    if (i_cache_el == (int)(Mesh::undef_idx))
        return_val = default_value_;
    else if ( std::isnan((*input_data_cache_)[i_cache_el]) )
        return_val = default_value_;
    else
        return_val = (*input_data_cache_)[i_cache_el];

    if ( std::isnan(return_val) ) {
        actual_compute_region_error = RegionValueErr(region_name, elm_idx, return_val);
    } else
        return_val *= this->unit_conversion_coefficient_;

    return return_val;
}



/*template <int spacedim, class Value>
Armor::ArmaMat<typename Value::element_type, Value::NRows_, Value::NCols_> FieldFE<spacedim, Value>::handle_fe_shape(unsigned int dim,
        unsigned int i_dof, unsigned int i_qp, unsigned int comp_index)
{
    Armor::ArmaMat<typename Value::element_type, Value::NCols_, Value::NRows_> v;
    for (unsigned int c=0; c<Value::NRows_*Value::NCols_; ++c)
        v(c/spacedim,c%spacedim) = fe_values_[dim].shape_value_component(i_dof, i_qp, comp_index+c);
    if (Value::NRows_ == Value::NCols_)
        return v;
    else
        return v.t();
}*/



template <int spacedim, class Value>
FieldFE<spacedim, Value>::~FieldFE()
{}


// Instantiations of FieldFE
INSTANCE_ALL(FieldFE)

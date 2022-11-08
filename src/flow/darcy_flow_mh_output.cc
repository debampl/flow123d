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
 * @file    darcy_flow_mh_output.cc
 * @ingroup flow
 * @brief   Output class for darcy_flow_mh model.
 * @author  Jan Brezina
 */

#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <system/global_defs.h>

#include "flow/darcy_flow_lmh.hh"
#include "flow/assembly_lmh.hh"
#include "flow/darcy_flow_mh_output.hh"

#include "io/output_time.hh"
#include "io/observe.hh"
#include "system/system.hh"
#include "system/sys_profiler.hh"
#include "system/index_types.hh"

#include "fields/field_set.hh"
#include "fem/dofhandler.hh"
#include "fem/fe_values.hh"
#include "fem/fe_rt.hh"
#include "fem/fe_values_views.hh"
#include "quadrature/quadrature_lib.hh"
#include "fields/field_fe.hh"
#include "fields/fe_value_handler.hh"
#include "fields/generic_field.hh"

#include "mesh/mesh.h"
#include "mesh/partitioning.hh"
#include "mesh/accessors.hh"
#include "mesh/node_accessor.hh"
#include "mesh/range_wrapper.hh"
#include "input/reader_to_storage.hh"

// #include "coupling/balance.hh"
#include "intersection/mixed_mesh_intersections.hh"
#include "intersection/intersection_local.hh"

namespace it = Input::Type;


const it::Instance & DarcyFlowMHOutput::get_input_type(FieldSet& eq_data, const std::string &equation_name) {
	OutputFields output_fields;
	output_fields += eq_data;
	return output_fields.make_output_type(equation_name, "");
}


const it::Instance & DarcyFlowMHOutput::get_input_type_specific() {
    
    static it::Record& rec = it::Record("Output_DarcyMHSpecific", "Specific Darcy flow MH output.")
        .copy_keys(OutputSpecificFields::get_input_type())
        .declare_key("compute_errors", it::Bool(), it::Default("false"),
                        "SPECIAL PURPOSE. Computes error norms of the solution, particulary suited for non-compatible coupling models.")
        .declare_key("raw_flow_output", it::FileName::output(), it::Default::optional(),
                        "Output file with raw data from MH module.")
        .close();
    
    OutputSpecificFields output_fields;
    return output_fields.make_output_type_from_record(rec,
                                                        "Flow_Darcy_MH_specific",
                                                        "");
}


DarcyFlowMHOutput::OutputFields::OutputFields()
: EquationOutput()
{

//     *this += field_ele_pressure.name("pressure_p0_old").units(UnitSI().m()) // TODO remove: obsolete field
//              .flags(FieldFlag::equation_result)
//              .description("Pressure solution - P0 interpolation.");
//     *this += field_node_pressure.name("pressure_p1").units(UnitSI().m())
//              .flags(FieldFlag::equation_result)
//              .description("Pressure solution - P1 interpolation.");
// 	*this += field_ele_piezo_head.name("piezo_head_p0_old").units(UnitSI().m()) // TODO remove: obsolete field
//              .flags(FieldFlag::equation_result)
//              .description("Piezo head solution - P0 interpolation.");
// 	*this += field_ele_flux.name("velocity_p0_old").units(UnitSI().m()) // TODO remove: obsolete field
//              .flags(FieldFlag::equation_result)
//              .description("Velocity solution - P0 interpolation.");
	*this += subdomain.name("subdomain")
					  .units( UnitSI::dimensionless() )
					  .flags(FieldFlag::equation_external_output)
                      .description("Subdomain ids of the domain decomposition.");
	*this += region_id.name("region_id")
	        .units( UnitSI::dimensionless())
	        .flags(FieldFlag::equation_external_output)
            .description("Region ids.");
}


DarcyFlowMHOutput::OutputSpecificFields::OutputSpecificFields()
: EquationOutput()
{
    *this += pressure_diff.name("pressure_diff").units(UnitSI().m())
             .flags(FieldFlag::equation_result) 
             .description("Error norm of the pressure solution. [Experimental]");
    *this += velocity_diff.name("velocity_diff").units(UnitSI().m().s(-1))
             .flags(FieldFlag::equation_result)
             .description("Error norm of the velocity solution. [Experimental]");
    *this += div_diff.name("div_diff").units(UnitSI().s(-1))
             .flags(FieldFlag::equation_result)
             .description("Error norm of the divergence of the velocity solution. [Experimental]");
}

DarcyFlowMHOutput::DarcyFlowMHOutput(DarcyLMH *flow, Input::Record main_mh_in_rec)
: darcy_flow(flow),
  mesh_(&darcy_flow->mesh()),
  compute_errors_(false),
  is_output_specific_fields(false)
{
    output_stream = OutputTime::create_output_stream("flow",
                                                     main_mh_in_rec.val<Input::Record>("output_stream"),
                                                     darcy_flow->time().get_unit_conversion());
    prepare_output(main_mh_in_rec);

    auto in_rec_specific = main_mh_in_rec.find<Input::Record>("output_specific");
    if (in_rec_specific) {
        in_rec_specific->opt_val("compute_errors", compute_errors_);
        
        // raw output
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (rank == 0) {
            
            // optionally open raw output file
            FilePath raw_output_file_path;
            if (in_rec_specific->opt_val("raw_flow_output", raw_output_file_path))
            {
                int mpi_size;
                MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
                if(mpi_size > 1)
                {
                    WarningOut() << "Raw output is not available in parallel computation. MPI size: " << mpi_size << "\n";
                }
                else
                {
                    MessageOut() << "Opening raw flow output: " << raw_output_file_path << "\n";
                    try {
                        raw_output_file_path.open_stream(raw_output_file);
                    } INPUT_CATCH(FilePath::ExcFileOpen, FilePath::EI_Address_String, (*in_rec_specific))
                }
            }
        }
        
        auto fields_array = in_rec_specific->val<Input::Array>("fields");
        if(fields_array.size() > 0){
            is_output_specific_fields = true;
            prepare_specific_output(*in_rec_specific);
        }
    }
}

void DarcyFlowMHOutput::prepare_output(Input::Record main_mh_in_rec)
{
  	// we need to add data from the flow equation at this point, not in constructor of OutputFields
	output_fields += darcy_flow->eq_fieldset();

    // read optional user fields
	// TODO: check of DarcyLMH type is temporary, remove condition at the same time as removal DarcyMH
	if(DarcyLMH* d = dynamic_cast<DarcyLMH*>(darcy_flow)) {
        Input::Array user_fields_arr;
        if (main_mh_in_rec.opt_val("user_fields", user_fields_arr)) {
            d->init_user_fields(user_fields_arr, this->output_fields);
        }
	}

    output_fields.set_mesh(*mesh_);

	output_fields.subdomain = GenericField<3>::subdomain(*mesh_);
	output_fields.region_id = GenericField<3>::region_id(*mesh_);

	Input::Record in_rec_output = main_mh_in_rec.val<Input::Record>("output");
	//output_stream->add_admissible_field_names(in_rec_output.val<Input::Array>("fields"));
	//output_stream->mark_output_times(darcy_flow->time());
    output_fields.initialize(output_stream, mesh_, in_rec_output, darcy_flow->time() );
}

void DarcyFlowMHOutput::prepare_specific_output(Input::Record in_rec)
{
    diff_eq_fields_ = darcy_flow->eq_fields_;
    diff_eq_data_ = std::make_shared<DiffEqData>();
    diff_eq_data_->flow_data_ = darcy_flow->eq_data_;
    ASSERT_PTR(diff_eq_data_->flow_data_);

    { // init DOF handlers represents element DOFs
        uint p_elem_component = 1;
        diff_eq_data_->dh_ = std::make_shared<SubDOFHandlerMultiDim>(diff_eq_data_->flow_data_->dh_, p_elem_component);
    }

    // mask 2d elements crossing 1d
    if (diff_eq_data_->flow_data_->mortar_method_ != DarcyLMH::NoMortar) {
        diff_eq_data_->velocity_mask.resize(mesh_->n_elements(),0);
        for(IntersectionLocal<1,2> & isec : mesh_->mixed_intersections().intersection_storage12_) {
            diff_eq_data_->velocity_mask[ isec.bulk_ele_idx() ]++;
        }
    }

    output_specific_fields.set_mesh(*mesh_);

    diff_eq_data_->vel_diff_ptr = create_field_fe<3, FieldValue<3>::Scalar>(diff_eq_data_->dh_);
    output_specific_fields.velocity_diff.set(diff_eq_data_->vel_diff_ptr, 0);
    diff_eq_data_->pressure_diff_ptr = create_field_fe<3, FieldValue<3>::Scalar>(diff_eq_data_->dh_);
    output_specific_fields.pressure_diff.set(diff_eq_data_->pressure_diff_ptr, 0);
    diff_eq_data_->div_diff_ptr = create_field_fe<3, FieldValue<3>::Scalar>(diff_eq_data_->dh_);
    output_specific_fields.div_diff.set(diff_eq_data_->div_diff_ptr, 0);

    output_specific_fields.set_time(darcy_flow->time().step(), LimitSide::right);
    output_specific_fields.initialize(output_stream, mesh_, in_rec, darcy_flow->time() );

    if (compute_errors_)
        set_specific_output_python_fields();
}

/*
 * Input string of specific output python fields.
 * Array of 3-items for 1D, 2D, 3D
 */
string spec_fields_input = R"YAML(
  - source_file: analytical_module.py
    class: AllValues1D
    used_fields: ["X"]
  - source_file: analytical_module.py
    class: AllValues2D
    used_fields: ["X"]
  - source_file: analytical_module.py
    class: AllValues3D
    used_fields: ["X"]
)YAML";


void DarcyFlowMHOutput::set_specific_output_python_fields()
{
	typedef FieldValue<3>::Scalar ScalarSolution;
	typedef FieldValue<3>::VectorFixed VectorSolution;

    static Input::Type::Array arr = Input::Type::Array( FieldPython<3,ScalarSolution>::get_input_type(), 3, 3 );

    // Create separate vectors of 1D, 2D, 3D regions
    std::vector< std::vector<std::string> > reg_by_dim(3);
    for (unsigned int i=1; i<2*mesh_->region_db().bulk_size(); i+=2) {
        unsigned int dim = mesh_->region_db().get_dim(i);
        ASSERT_GT(dim, 0).error("Bulk region with dim==0!\n");
        reg_by_dim[dim-1].push_back( mesh_->region_db().get_label(i) );
    }

    Input::ReaderToStorage reader( spec_fields_input, arr, Input::FileFormat::format_YAML );
    Input::Array in_arr=reader.get_root_interface<Input::Array>();
    std::vector<Input::Record> in_recs;
    in_arr.copy_to(in_recs);

    // Create instances of FieldPython and set them to Field objects
    std::string source_file = "analytical_module.py";
    for (uint i_dim=0; i_dim<3; ++i_dim) {
        this->set_ref_solution<ScalarSolution>(in_recs[i_dim], diff_eq_fields_->ref_pressure, reg_by_dim[i_dim]);
        this->set_ref_solution<VectorSolution>(in_recs[i_dim], diff_eq_fields_->ref_velocity, reg_by_dim[i_dim]);
        this->set_ref_solution<ScalarSolution>(in_recs[i_dim], diff_eq_fields_->ref_divergence, reg_by_dim[i_dim]);
    }
}


template <class FieldType>
void DarcyFlowMHOutput::set_ref_solution(const Input::Record &in_rec, Field<3, FieldType> &output_field, std::vector<std::string> reg) {
    FieldAlgoBaseInitData init_data(output_field.input_name(), output_field.n_comp(), output_field.units(), output_field.limits(), output_field.flags());

    std::shared_ptr< FieldPython<3, FieldType> > algo = std::make_shared< FieldPython<3, FieldType> >();
    algo->init_from_input( in_rec, init_data );
    output_field.set(algo, darcy_flow->time().t(), reg);
}

#define DARCY_SET_REF_SOLUTION(FTYPE) \
template void DarcyFlowMHOutput::set_ref_solution<FTYPE>(const Input::Record &, Field<3, FTYPE> &, std::vector<std::string>)

DARCY_SET_REF_SOLUTION(FieldValue<3>::Scalar);
DARCY_SET_REF_SOLUTION(FieldValue<3>::VectorFixed);


DarcyFlowMHOutput::~DarcyFlowMHOutput()
{}





//=============================================================================
// CONVERT SOLUTION, CALCULATE BALANCES, ETC...
//=============================================================================


void DarcyFlowMHOutput::output()
{
    START_TIMER("Darcy fields output");

    {
        START_TIMER("post-process output fields");

        {
                  output_internal_flow_data();
        }
    }

    {
        START_TIMER("evaluate output fields");
        output_fields.set_time(darcy_flow->time().step(), LimitSide::right);
        output_fields.output(darcy_flow->time().step());
    }
    
    if (compute_errors_)
    {
        START_TIMER("compute specific output fields");
        compute_l2_difference();
    }
    
    if(is_output_specific_fields)
    {
        START_TIMER("evaluate output fields");
        output_specific_fields.set_time(darcy_flow->time().step(), LimitSide::right);
        output_specific_fields.output(darcy_flow->time().step());
    }

    
}



/*
 * Output of internal flow data.
 */
void DarcyFlowMHOutput::output_internal_flow_data()
{
    START_TIMER("DarcyFlowMHOutput::output_internal_flow_data");

    if (! raw_output_file.is_open()) return;
    
    //char dbl_fmt[ 16 ]= "%.8g ";
    // header
    raw_output_file <<  "// fields:\n//ele_id    ele_presure    flux_in_barycenter[3]    n_sides   side_pressures[n]    side_fluxes[n]\n";
    raw_output_file <<  fmt::format("$FlowField\nT={}\n", darcy_flow->time().t());
    raw_output_file <<  fmt::format("{}\n" , mesh_->n_elements() );

    
    DarcyLMH::EqFields* eq_fields = nullptr;
    DarcyLMH::EqData* eq_data = nullptr;
    if (DarcyLMH* d = dynamic_cast<DarcyLMH*>(darcy_flow))
    {
        eq_fields = d->eq_fields_.get();
        eq_data = d->eq_data_.get();
    }
    ASSERT_PTR(eq_data);
    
    arma::vec3 flux_in_center;
    
    auto permutation_vec = eq_data->dh_->mesh()->element_permutations();
    for (unsigned int i_elem=0; i_elem<eq_data->dh_->n_own_cells(); ++i_elem) {
        ElementAccessor<3> ele(eq_data->dh_->mesh(), permutation_vec[i_elem]);
        DHCellAccessor dh_cell = eq_data->dh_->cell_accessor_from_element( ele.idx() );
        LocDofVec indices = dh_cell.get_loc_dof_indices();

        std::stringstream ss;
        // pressure
        ss << fmt::format("{} {} ", dh_cell.elm().input_id(), eq_data->full_solution.get(indices[ele->n_sides()]));
        
        // velocity at element center
        flux_in_center = eq_fields->field_ele_velocity.value(ele.centre(), ele);
        for (unsigned int i = 0; i < 3; i++)
        	ss << flux_in_center[i] << " ";

        // number of sides
        ss << ele->n_sides() << " ";

        // use node permutation to permute sides
        auto &new_to_old_node = ele.orig_nodes_order();
        std::vector<uint> old_to_new_side(ele->n_sides());
        for (unsigned int i = 0; i < ele->n_sides(); i++) {
            // According to RefElement<dim>::opposite_node()
            uint new_opp_node = ele->n_sides() - i - 1;
            uint old_opp_node = new_to_old_node[new_opp_node];
            uint old_iside = ele->n_sides() - old_opp_node - 1;
            old_to_new_side[old_iside] = i;
        }
        
        // pressure on edges
        // unsigned int lid = ele->n_sides() + 1;
        for (unsigned int i = 0; i < ele->n_sides(); i++) {
            uint new_lid = ele->n_sides() + 1 + old_to_new_side[i];
            ss << eq_data->full_solution.get(indices[new_lid]) << " ";
        }
        // fluxes on sides
        for (unsigned int i = 0; i < ele->n_sides(); i++) {
            uint new_iside = old_to_new_side[i];
            ss << eq_data->full_solution.get(indices[new_iside]) << " ";
        }
        
        // remove last white space
        string line = ss.str();
        raw_output_file << line.substr(0, line.size()-1) << endl;
    }    
    
    raw_output_file << "$EndFlowField\n" << endl;
}


#include "quadrature/quadrature_lib.hh"
#include "fem/fe_p.hh"
#include "fem/fe_values.hh"
#include "fields/field_python.hh"
#include "fields/field_values.hh"

typedef FieldPython<3, FieldValue<3>::Vector > ExactSolution;

/*
* Calculate approximation of L2 norm for:
 * 1) difference between regularized pressure and analytical solution (using FunctionPython)
 * 2) difference between RT velocities and analytical solution
 * 3) difference of divergence
 * */

void DarcyFlowMHOutput::l2_diff_local(DHCellAccessor dh_cell,
                   FEValues<3> &fe_values, FEValues<3> &fv_rt) {

    ASSERT( fe_values.dim() == fv_rt.dim());
    unsigned int dim = fe_values.dim();

    ElementAccessor<3> ele = dh_cell.elm();
    fv_rt.reinit(ele);
    fe_values.reinit(ele);
    
    double conductivity = diff_eq_fields_->conductivity.value(ele.centre(), ele );
    double cross = diff_eq_fields_->cross_section.value(ele.centre(), ele );


    // get coefficients on the current element
    vector<double> fluxes(dim+1);
//     vector<double> pressure_traces(dim+1);

    for (unsigned int li = 0; li < ele->n_sides(); li++) {
        fluxes[li] = diff_eq_data_->flow_data_->full_solution.get( dh_cell.get_loc_dof_indices()[li] );
//         pressure_traces[li] = diff_eq_data_->dh->side_scalar( *(ele->side( li ) ) );
    }
    const uint ndofs = dh_cell.n_dofs();
    // TODO: replace with DHCell getter when available for FESystem component
    double pressure_mean = diff_eq_data_->flow_data_->full_solution.get( dh_cell.get_loc_dof_indices()[ndofs/2] );

    arma::vec3 flux_in_q_point;
    arma::vec3 ref_flux;
    double ref_pressure, ref_divergence;

    double velocity_diff=0, divergence_diff=0, pressure_diff=0, diff;

    // 1d:  mean_x_squared = 1/6 (v0^2 + v1^2 + v0*v1)
    // 2d:  mean_x_squared = 1/12 (v0^2 + v1^2 +v2^2 + v0*v1 + v0*v2 + v1*v2)
    double mean_x_squared=0;
    for(unsigned int i_node=0; i_node < ele->n_nodes(); i_node++ )
        for(unsigned int j_node=0; j_node < ele->n_nodes(); j_node++ )
        {
            mean_x_squared += (i_node == j_node ? 2.0 : 1.0) / ( 6 * dim )   // multiply by 2 on diagonal
                    * arma::dot( *ele.node(i_node), *ele.node(j_node));
        }

    for(unsigned int i_point=0; i_point < fe_values.n_points(); i_point++) {
        arma::vec3 q_point = fe_values.point(i_point);


        ref_pressure = diff_eq_fields_->ref_pressure.value(q_point, ele );
        ref_flux = diff_eq_fields_->ref_velocity.value(q_point, ele );
        ref_divergence = diff_eq_fields_->ref_divergence.value(q_point, ele );


        // compute postprocesed pressure
        diff = 0;
        for(unsigned int i_shape=0; i_shape < ele->n_sides(); i_shape++) {
            unsigned int oposite_node = 0;
            switch (dim) {
                case 1: oposite_node =  RefElement<1>::oposite_node(i_shape); break;
                case 2: oposite_node =  RefElement<2>::oposite_node(i_shape); break;
                case 3: oposite_node =  RefElement<3>::oposite_node(i_shape); break;
                default: ASSERT_PERMANENT(false)(dim).error("Unsupported FE dimension."); break;
            }

            diff += fluxes[ i_shape ] *
                               (  arma::dot( q_point, q_point )/ 2
                                - mean_x_squared / 2
                                - arma::dot( q_point, *ele.node(oposite_node) )
                                + arma::dot( ele.centre(), *ele.node(oposite_node) )
                               );
        }

        diff = - (1.0 / conductivity) * diff / dim / ele.measure() / cross + pressure_mean ;
        diff = ( diff - ref_pressure);
        pressure_diff += diff * diff * fe_values.JxW(i_point);


        // velocity difference
        flux_in_q_point.zeros();
        for(unsigned int i_shape=0; i_shape < ele->n_sides(); i_shape++) {
            flux_in_q_point += fluxes[ i_shape ]
                              * fv_rt.vector_view(0).value(i_shape, i_point)
                              / cross;
        }

        flux_in_q_point -= ref_flux;
        velocity_diff += dot(flux_in_q_point, flux_in_q_point) * fe_values.JxW(i_point);

        // divergence diff
        diff = 0;
        for(unsigned int i_shape=0; i_shape < ele->n_sides(); i_shape++) diff += fluxes[ i_shape ];
        diff = ( diff / ele.measure() / cross - ref_divergence);
        divergence_diff += diff * diff * fe_values.JxW(i_point);

    }

    // DHCell constructed with diff fields DH, get DOF indices of actual element
    DHCellAccessor sub_dh_cell = dh_cell.cell_with_other_dh(diff_eq_data_->dh_.get());
    IntIdx idx = sub_dh_cell.get_loc_dof_indices()[0];

    auto velocity_data = diff_eq_data_->vel_diff_ptr->vec();
    velocity_data.set( idx, sqrt(velocity_diff) );
    diff_eq_data_->velocity_error[dim-1] += velocity_diff;
    if (dim == 2 && diff_eq_data_->velocity_mask.size() != 0 ) {
        diff_eq_data_->mask_vel_error += (diff_eq_data_->velocity_mask[ ele.idx() ])? 0 : velocity_diff;
    }

    auto pressure_data = diff_eq_data_->pressure_diff_ptr->vec();
    pressure_data.set( idx, sqrt(pressure_diff) );
    diff_eq_data_->pressure_error[dim-1] += pressure_diff;

    auto div_data = diff_eq_data_->div_diff_ptr->vec();
    div_data.set( idx, sqrt(divergence_diff) );
    diff_eq_data_->div_error[dim-1] += divergence_diff;

}

DarcyFlowMHOutput::FEData::FEData()
: order(4),
  quad(QGauss::make_array(order)),
  fe_p1(0), fe_p0(0),
  fe_rt( )
{
    UpdateFlags flags = update_values | update_JxW_values | update_quadrature_points;
    fe_values = mixed_fe_values(quad, fe_p0, flags);
    fv_rt = mixed_fe_values(quad, fe_rt, flags);
}


void DarcyFlowMHOutput::compute_l2_difference() {
	DebugOut() << "l2 norm output\n";
    ofstream os( FilePath("solution_error", FilePath::output_file) );

    diff_eq_data_->mask_vel_error=0;
    for(unsigned int j=0; j<3; j++){
        diff_eq_data_->pressure_error[j] = 0;
        diff_eq_data_->velocity_error[j] = 0;
        diff_eq_data_->div_error[j] = 0;
    }

    //diff_eq_data_->ele_flux = &( ele_flux );

    for (DHCellAccessor dh_cell : diff_eq_data_->flow_data_->dh_->own_range()) {

    	switch (dh_cell.dim()) {
        case 1:
            l2_diff_local( dh_cell, fe_data.fe_values[1], fe_data.fv_rt[1]);
            break;
        case 2:
            l2_diff_local( dh_cell, fe_data.fe_values[2], fe_data.fv_rt[2]);
            break;
        case 3:
            l2_diff_local( dh_cell, fe_data.fe_values[3], fe_data.fv_rt[3]);
            break;
        }
    }
    
    // square root for L2 norm
    for(unsigned int j=0; j<3; j++){
        diff_eq_data_->pressure_error[j] = sqrt(diff_eq_data_->pressure_error[j]);
        diff_eq_data_->velocity_error[j] = sqrt(diff_eq_data_->velocity_error[j]);
        diff_eq_data_->div_error[j] = sqrt(diff_eq_data_->div_error[j]);
    }
    diff_eq_data_->mask_vel_error = sqrt(diff_eq_data_->mask_vel_error);

    os 	<< "l2 norm output\n\n"
    	<< "pressure error 1d: " << diff_eq_data_->pressure_error[0] << endl
    	<< "pressure error 2d: " << diff_eq_data_->pressure_error[1] << endl
    	<< "pressure error 3d: " << diff_eq_data_->pressure_error[2] << endl
    	<< "velocity error 1d: " << diff_eq_data_->velocity_error[0] << endl
    	<< "velocity error 2d: " << diff_eq_data_->velocity_error[1] << endl
    	<< "velocity error 3d: " << diff_eq_data_->velocity_error[2] << endl
    	<< "masked velocity error 2d: " << diff_eq_data_->mask_vel_error <<endl
    	<< "div error 1d: " << diff_eq_data_->div_error[0] << endl
    	<< "div error 2d: " << diff_eq_data_->div_error[1] << endl
        << "div error 3d: " << diff_eq_data_->div_error[2];
}



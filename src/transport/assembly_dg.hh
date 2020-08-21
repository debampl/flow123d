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
 * @file    assembly_dg.hh
 * @brief
 */

#ifndef ASSEMBLY_DG_HH_
#define ASSEMBLY_DG_HH_

#include "coupling/generic_assembly.hh"
#include "coupling/assembly_base.hh"
#include "transport/transport_dg.hh"
#include "fem/fe_p.hh"
#include "fem/fe_values.hh"
#include "quadrature/quadrature_lib.hh"
#include "coupling/balance.hh"
#include "fields/field_value_cache.hh"



/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim, class Model>
class MassAssemblyDG : public AssemblyBase<dim>
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    MassAssemblyDG(EqDataDG *data)
    : AssemblyBase<dim>(data->dg_order), data_(data) {}

    /// Destructor.
    ~MassAssemblyDG() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(std::shared_ptr<Balance> balance) {
        this->balance_ = balance;

        fe_ = std::make_shared< FE_P_disc<dim> >(data_->dg_order);
        fe_values_.initialize(*this->quad_, *fe_, update_values | update_gradients | update_JxW_values | update_quadrature_points);
        ndofs_ = fe_->n_dofs();
        dof_indices_.resize(ndofs_);
        local_matrix_.resize(4*ndofs_*ndofs_);
        local_retardation_balance_vector_.resize(ndofs_);
        local_mass_balance_vector_.resize(ndofs_);

    }


    /// Assemble integral over element
    void assemble_volume_integrals(DHCellAccessor cell) override
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");
        ElementAccessor<3> elm = cell.elm();
        unsigned int k;

        fe_values_.reinit(elm);
        cell.get_dof_indices(dof_indices_);

        for (unsigned int sbi=0; sbi<data_->n_substances(); ++sbi)
        {
            // assemble the local mass matrix
            for (unsigned int i=0; i<ndofs_; i++)
            {
                for (unsigned int j=0; j<ndofs_; j++)
                {
                    local_matrix_[i*ndofs_+j] = 0;
                    k=0;
                    for (auto p : data_->mass_assembly_->bulk_points(dim, cell) )
                    {
                        local_matrix_[i*ndofs_+j] += (data_->mass_matrix_coef(p)+data_->retardation_coef[sbi](p)) *
                                fe_values_.shape_value(j,k)*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                        k++;
                    }
                }
            }

            for (unsigned int i=0; i<ndofs_; i++)
            {
                local_mass_balance_vector_[i] = 0;
                local_retardation_balance_vector_[i] = 0;
                k=0;
                for (auto p : data_->mass_assembly_->bulk_points(dim, cell) )
                {
                    local_mass_balance_vector_[i] += data_->mass_matrix_coef(p)*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                    local_retardation_balance_vector_[i] -= data_->retardation_coef[sbi](p)*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                    k++;
                }
            }

            balance_->add_mass_values(data_->subst_idx()[sbi], cell, cell.get_loc_dof_indices(),
                                               local_mass_balance_vector_, 0);

            data_->ls_dt[sbi]->mat_set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]));
            VecSetValues(data_->ret_vec[sbi], ndofs_, &(dof_indices_[0]), &(local_retardation_balance_vector_[0]), ADD_VALUES);
        }
    }

    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        balance_->start_mass_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        balance_->finish_mass_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::reallocate_cache.
    void reallocate_cache(const ElementCacheMap &cache_map) override
    {
        data_->cache_reallocate(cache_map);
    }


    private:
        shared_ptr<FiniteElement<dim>> fe_;                    ///< Finite element for the solution of the advection-diffusion equation.

        /// Pointer to balance object
        std::shared_ptr<Balance> balance_;

        /// Data object shared with TransportDG
        EqDataDG *data_;

        unsigned int ndofs_;                                      ///< Number of dofs
        FEValues<3> fe_values_;                                   ///< FEValues of object (of P disc finite element type)

        vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
        vector<PetscScalar> local_matrix_;                        ///< Auxiliary vector for assemble methods
        vector<PetscScalar> local_retardation_balance_vector_;    ///< Auxiliary vector for assemble mass matrix.
        vector<PetscScalar> local_mass_balance_vector_;           ///< Same as previous.

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim, class Model>
class StiffnessAssemblyDG : public AssemblyBase<dim>
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    StiffnessAssemblyDG(EqDataDG *data)
    : AssemblyBase<dim>(data->dg_order), data_(data) {}

    /// Destructor.
    ~StiffnessAssemblyDG() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(FMT_UNUSED std::shared_ptr<Balance> balance) {
        fe_ = std::make_shared< FE_P_disc<dim> >(data_->dg_order);
        fe_low_ = std::make_shared< FE_P_disc<dim-1> >(data_->dg_order);
        fe_values_.initialize(*this->quad_, *fe_, update_values | update_gradients | update_JxW_values | update_quadrature_points);
        if (dim>1) {
            fe_values_vb_.initialize(*this->quad_low_, *fe_low_, update_values | update_gradients | update_JxW_values | update_quadrature_points);
        }
        fe_values_side_.initialize(*this->quad_low_, *fe_, update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points);
        ndofs_ = fe_->n_dofs();
        qsize_lower_dim_ = this->quad_low_->size();
        dof_indices_.resize(ndofs_);
        side_dof_indices_vb_.resize(2*ndofs_);
        local_matrix_.resize(4*ndofs_*ndofs_);

        fe_values_vec_.resize(data_->max_edg_sides);
        for (unsigned int sid=0; sid<data_->max_edg_sides; sid++)
        {
            side_dof_indices_.push_back( vector<LongIdx>(ndofs_) );
            fe_values_vec_[sid].initialize(*this->quad_low_, *fe_,
                    update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points);
        }

        // index 0 = element with lower dimension,
        // index 1 = side of element with higher dimension
        fv_sb_.resize(2);
        fv_sb_[0] = &fe_values_vb_;
        fv_sb_[1] = &fe_values_side_;
    }


    /// Assembles the volume integrals into the stiffness matrix.
    void assemble_volume_integrals(DHCellAccessor cell) override
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");
        if (!cell.is_own()) return;

        ElementAccessor<3> elm = cell.elm();

        fe_values_.reinit(elm);
        cell.get_dof_indices(dof_indices_);
        unsigned int k;

        // assemble the local stiffness matrix
        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            for (unsigned int i=0; i<ndofs_; i++)
                for (unsigned int j=0; j<ndofs_; j++)
                    local_matrix_[i*ndofs_+j] = 0;

            k=0;
            for (auto p : data_->stiffness_assembly_->bulk_points(dim, cell) )
            {
                for (unsigned int i=0; i<ndofs_; i++)
                {
                    arma::vec3 Kt_grad_i = data_->diffusion_coef[sbi](p).t()*fe_values_.shape_grad(i,k);
                    double ad_dot_grad_i = arma::dot(data_->advection_coef[sbi](p), fe_values_.shape_grad(i,k));

                    for (unsigned int j=0; j<ndofs_; j++)
                        local_matrix_[i*ndofs_+j] += (arma::dot(Kt_grad_i, fe_values_.shape_grad(j,k))
                                                  -fe_values_.shape_value(j,k)*ad_dot_grad_i
                                                  +data_->sources_sigma_out[sbi](p)*fe_values_.shape_value(j,k)*fe_values_.shape_value(i,k))*fe_values_.JxW(k);
                }
                k++;
            }
            data_->ls[sbi]->mat_set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]));
        }
    }


    /// Assembles the fluxes on the boundary.
    void assemble_fluxes_boundary(DHCellSide cell_side, FMT_UNUSED const TimeStep &step) override
    {
        ASSERT_EQ_DBG(cell_side.dim(), dim).error("Dimension of element mismatch!");
        if (!cell_side.cell().is_own()) return;

        Side side = cell_side.side();
        const DHCellAccessor &cell = cell_side.cell();

        cell.get_dof_indices(dof_indices_);
        fe_values_side_.reinit(side);
        unsigned int k;
        double gamma_l;

        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            std::fill(local_matrix_.begin(), local_matrix_.end(), 0);

            // On Neumann boundaries we have only term from integrating by parts the advective term,
            // on Dirichlet boundaries we additionally apply the penalty which enforces the prescribed value.
            double side_flux = 0;
            k=0;
            for (auto p : data_->stiffness_assembly_->boundary_points(dim, cell_side) ) {
                side_flux += arma::dot(data_->advection_coef[sbi](p), fe_values_side_.normal_vector(k))*fe_values_side_.JxW(k);
                k++;
            }
            double transport_flux = side_flux/side.measure();

            auto p_side = *( data_->stiffness_assembly_->boundary_points(dim, cell_side).begin() );
            auto p_bdr = p_side.point_bdr( side.cond().element_accessor() );
            unsigned int bc_type = data_->bc_type[sbi](p_bdr);
            if (bc_type == AdvectionDiffusionModel::abc_dirichlet)
            {
                // set up the parameters for DG method
                k=0; // temporary solution, set dif_coef until set_DG_parameters_boundary will not be removed
                for (auto p : data_->stiffness_assembly_->boundary_points(dim, cell_side) ) {
                    data_->dif_coef[sbi][k] = data_->diffusion_coef[sbi](p);
                    k++;
                }
                data_->set_DG_parameters_boundary(side, qsize_lower_dim_, data_->dif_coef[sbi], transport_flux, fe_values_side_.normal_vector(0), data_->dg_penalty[sbi](p_side), gamma_l);
                data_->gamma[sbi][side.cond_idx()] = gamma_l;
                transport_flux += gamma_l;
            }

            // fluxes and penalty
            k=0;
            for (auto p : data_->stiffness_assembly_->boundary_points(dim, cell_side) )
            {
                double flux_times_JxW;
                if (bc_type == AdvectionDiffusionModel::abc_total_flux)
                {
                    //sigma_ corresponds to robin_sigma
                    auto p_bdr = p.point_bdr(side.cond().element_accessor());
                    flux_times_JxW = data_->cross_section(p)*data_->bc_robin_sigma[sbi](p_bdr)*fe_values_side_.JxW(k);
                }
                else if (bc_type == AdvectionDiffusionModel::abc_diffusive_flux)
                {
                    auto p_bdr = p.point_bdr(side.cond().element_accessor());
                    flux_times_JxW = (transport_flux + data_->cross_section(p)*data_->bc_robin_sigma[sbi](p_bdr))*fe_values_side_.JxW(k);
                }
                else if (bc_type == AdvectionDiffusionModel::abc_inflow && side_flux < 0)
                    flux_times_JxW = 0;
                else
                    flux_times_JxW = transport_flux*fe_values_side_.JxW(k);

                for (unsigned int i=0; i<ndofs_; i++)
                {
                    for (unsigned int j=0; j<ndofs_; j++)
                    {
                        // flux due to advection and penalty
                        local_matrix_[i*ndofs_+j] += flux_times_JxW*fe_values_side_.shape_value(i,k)*fe_values_side_.shape_value(j,k);

                        // flux due to diffusion (only on dirichlet and inflow boundary)
                        if (bc_type == AdvectionDiffusionModel::abc_dirichlet)
                            local_matrix_[i*ndofs_+j] -= (arma::dot(data_->diffusion_coef[sbi](p)*fe_values_side_.shape_grad(j,k),fe_values_side_.normal_vector(k))*fe_values_side_.shape_value(i,k)
                                    + arma::dot(data_->diffusion_coef[sbi](p)*fe_values_side_.shape_grad(i,k),fe_values_side_.normal_vector(k))*fe_values_side_.shape_value(j,k)*data_->dg_variant
                                    )*fe_values_side_.JxW(k);
                    }
                }
                k++;
            }

            data_->ls[sbi]->mat_set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]));
        }
    }


    /// Assembles the fluxes between elements of the same dimension.
    void assemble_fluxes_element_element(RangeConvert<DHEdgeSide, DHCellSide> edge_side_range) override {
        ASSERT_EQ_DBG(edge_side_range.begin()->element().dim(), dim).error("Dimension of element mismatch!");

        unsigned int k;
        double gamma_l, omega[2], transport_flux, delta[2], delta_sum;
        double aniso1, aniso2;
        double local_alpha=0.0;
        int sid=0, s1, s2;
        for( DHCellSide edge_side : edge_side_range )
        {
            auto dh_edge_cell = data_->dh_->cell_accessor_from_element( edge_side.elem_idx() );
            dh_edge_cell.get_dof_indices(side_dof_indices_[sid]);
            fe_values_vec_[sid].reinit(edge_side.side());
            ++sid;
        }
        arma::vec3 normal_vector = fe_values_vec_[0].normal_vector(0);

        // fluxes and penalty
        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            vector<double> fluxes(edge_side_range.begin()->n_edge_sides());
            double pflux = 0, nflux = 0; // calculate the total in- and out-flux through the edge
            sid=0;
            for( DHCellSide edge_side : edge_side_range )
            {
                fluxes[sid] = 0;
                k=0;
                for (auto p : data_->stiffness_assembly_->edge_points(dim, edge_side) ) {
                    fluxes[sid] += arma::dot(data_->advection_coef[sbi](p), fe_values_vec_[sid].normal_vector(k))*fe_values_vec_[sid].JxW(k);
                    k++;
                }
                fluxes[sid] /= edge_side.measure();
                if (fluxes[sid] > 0)
                    pflux += fluxes[sid];
                else
                    nflux += fluxes[sid];
                ++sid;
            }

            s1=0;
            for( DHCellSide edge_side1 : edge_side_range )
            {
                s2=-1; // need increment at begin of loop (see conditionally 'continue' directions)
                for( DHCellSide edge_side2 : edge_side_range )
                {
                    s2++;
                    if (s2<=s1) continue;
                    ASSERT(edge_side1.is_valid()).error("Invalid side of edge.");

                    arma::vec3 nv = fe_values_vec_[s1].normal_vector(0);

                    // set up the parameters for DG method
                    // calculate the flux from edge_side1 to edge_side2
                    if (fluxes[s2] > 0 && fluxes[s1] < 0)
                        transport_flux = fluxes[s1]*fabs(fluxes[s2]/pflux);
                    else if (fluxes[s2] < 0 && fluxes[s1] > 0)
                        transport_flux = fluxes[s1]*fabs(fluxes[s2]/nflux);
                    else
                        transport_flux = 0;

                    gamma_l = 0.5*fabs(transport_flux);

                    delta[0] = 0;
                    delta[1] = 0;
                    for (auto p1 : data_->stiffness_assembly_->edge_points(dim, edge_side1) )
                    {
                        auto p2 = p1.point_on(edge_side2);
                        delta[0] += dot(data_->diffusion_coef[sbi](p1)*normal_vector,normal_vector);
                        delta[1] += dot(data_->diffusion_coef[sbi](p2)*normal_vector,normal_vector);
                        local_alpha = max(data_->dg_penalty[sbi](p1), data_->dg_penalty[sbi](p2));
                    }
                    delta[0] /= qsize_lower_dim_;
                    delta[1] /= qsize_lower_dim_;

                    delta_sum = delta[0] + delta[1];

//                        if (delta_sum > numeric_limits<double>::epsilon())
                    if (fabs(delta_sum) > 0)
                    {
                        omega[0] = delta[1]/delta_sum;
                        omega[1] = delta[0]/delta_sum;
                        double h = edge_side1.diameter();
                        aniso1 = data_->elem_anisotropy(edge_side1.element());
                        aniso2 = data_->elem_anisotropy(edge_side2.element());
                        gamma_l += local_alpha/h*aniso1*aniso2*(delta[0]*delta[1]/delta_sum);
                    }
                    else
                        for (int i=0; i<2; i++) omega[i] = 0;
                    // end of set up the parameters for DG method

                    int sd[2]; bool is_side_own[2];
                    sd[0] = s1; is_side_own[0] = edge_side1.cell().is_own();
                    sd[1] = s2; is_side_own[1] = edge_side2.cell().is_own();

#define AVERAGE(i,k,side_id)  (fe_values_vec_[sd[side_id]].shape_value(i,k)*0.5)
#define JUMP(i,k,side_id)     ((side_id==0?1:-1)*fe_values_vec_[sd[side_id]].shape_value(i,k))

                    // For selected pair of elements:
                    for (int n=0; n<2; n++)
                    {
                        if (!is_side_own[n]) continue;

                        for (int m=0; m<2; m++)
                        {
                            for (unsigned int i=0; i<fe_values_vec_[sd[n]].n_dofs(); i++)
                                for (unsigned int j=0; j<fe_values_vec_[sd[m]].n_dofs(); j++)
                                    local_matrix_[i*fe_values_vec_[sd[m]].n_dofs()+j] = 0;

                            k=0;
                            for (auto p1 : data_->stiffness_assembly_->edge_points(dim, edge_side1) )
                            {
                                auto p2 = p1.point_on(edge_side2);
                                double flux_times_JxW = transport_flux*fe_values_vec_[0].JxW(k);
                                double gamma_times_JxW = gamma_l*fe_values_vec_[0].JxW(k);

                                for (unsigned int i=0; i<fe_values_vec_[sd[n]].n_dofs(); i++)
                                {
                                    double flux_JxW_jump_i = flux_times_JxW*JUMP(i,k,n);
                                    double gamma_JxW_jump_i = gamma_times_JxW*JUMP(i,k,n);
                                    double JxW_jump_i = fe_values_vec_[0].JxW(k)*JUMP(i,k,n);
                                    double JxW_var_wavg_i = fe_values_vec_[0].JxW(k) *
                                            arma::dot(data_->diffusion_coef[sbi]( (n==0 ? p1 : p2) )*fe_values_vec_[sd[n]].shape_grad(i,k),nv) *
                                            omega[n] * data_->dg_variant;

                                    for (unsigned int j=0; j<fe_values_vec_[sd[m]].n_dofs(); j++)
                                    {
                                        int index = i*fe_values_vec_[sd[m]].n_dofs()+j;

                                        // flux due to transport (applied on interior edges) (average times jump)
                                        local_matrix_[index] += flux_JxW_jump_i*AVERAGE(j,k,m);

                                        // penalty enforcing continuity across edges (applied on interior and Dirichlet edges) (jump times jump)
                                        local_matrix_[index] += gamma_JxW_jump_i*JUMP(j,k,m);

                                        // terms due to diffusion
                                        local_matrix_[index] -= arma::dot(data_->diffusion_coef[sbi]( (m==0 ? p1 : p2) )*fe_values_vec_[sd[m]].shape_grad(j,k),nv) * omega[m] * JxW_jump_i;
                                        local_matrix_[index] -= JUMP(j,k,m)*JxW_var_wavg_i;
                                    }
                                }
                                k++;
                            }
                            data_->ls[sbi]->mat_set_values(fe_values_vec_[sd[n]].n_dofs(), &(side_dof_indices_[sd[n]][0]), fe_values_vec_[sd[m]].n_dofs(), &(side_dof_indices_[sd[m]][0]), &(local_matrix_[0]));
                        }
                    }
#undef AVERAGE
#undef JUMP
                }
            s1++;
            }
        }
    }


    /// Assembles the fluxes between elements of different dimensions.
    void assemble_fluxes_element_side(DHCellAccessor cell_lower_dim, DHCellSide neighb_side) override {
        if (dim == 1) return;
        ASSERT_EQ_DBG(cell_lower_dim.dim(), dim-1).error("Dimension of element mismatch!");

        // Note: use data members csection_ and velocity_ for appropriate quantities of lower dim element

        double comm_flux[2][2];
        unsigned int n_dofs[2];
        ElementAccessor<3> elm_lower_dim = cell_lower_dim.elm();
        unsigned int n_indices = cell_lower_dim.get_dof_indices(dof_indices_);
        for(unsigned int i=0; i<n_indices; ++i) {
            side_dof_indices_vb_[i] = dof_indices_[i];
        }
        fe_values_vb_.reinit(elm_lower_dim);
        n_dofs[0] = fv_sb_[0]->n_dofs();

        DHCellAccessor cell_higher_dim = data_->dh_->cell_accessor_from_element( neighb_side.element().idx() );
        n_indices = cell_higher_dim.get_dof_indices(dof_indices_);
        for(unsigned int i=0; i<n_indices; ++i) {
            side_dof_indices_vb_[i+n_dofs[0]] = dof_indices_[i];
        }
        fe_values_side_.reinit(neighb_side.side());
        n_dofs[1] = fv_sb_[1]->n_dofs();

        // Testing element if they belong to local partition.
        bool own_element_id[2];
        own_element_id[0] = cell_lower_dim.is_own();
        own_element_id[1] = cell_higher_dim.is_own();

        unsigned int k;
        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++) // Optimize: SWAP LOOPS
        {
            for (unsigned int i=0; i<n_dofs[0]+n_dofs[1]; i++)
                for (unsigned int j=0; j<n_dofs[0]+n_dofs[1]; j++)
                    local_matrix_[i*(n_dofs[0]+n_dofs[1])+j] = 0;

            // set transmission conditions
            k=0;
            for (auto p_high : data_->stiffness_assembly_->coupling_points(dim, neighb_side) )
            {
                auto p_low = p_high.lower_dim(cell_lower_dim);
                // The communication flux has two parts:
                // - "diffusive" term containing sigma
                // - "advective" term representing usual upwind
                //
                // The calculation differs from the reference manual, since ad_coef and dif_coef have different meaning
                // than b and A in the manual.
                // In calculation of sigma there appears one more csection_lower in the denominator.
                double sigma = data_->fracture_sigma[sbi](p_low)*arma::dot(data_->diffusion_coef[sbi](p_low)*fe_values_side_.normal_vector(k),fe_values_side_.normal_vector(k))*
                        2*data_->cross_section(p_high)*data_->cross_section(p_high)/(data_->cross_section(p_low)*data_->cross_section(p_low));

                double transport_flux = arma::dot(data_->advection_coef[sbi](p_high), fe_values_side_.normal_vector(k));

                comm_flux[0][0] =  (sigma-min(0.,transport_flux))*fv_sb_[0]->JxW(k);
                comm_flux[0][1] = -(sigma-min(0.,transport_flux))*fv_sb_[0]->JxW(k);
                comm_flux[1][0] = -(sigma+max(0.,transport_flux))*fv_sb_[0]->JxW(k);
                comm_flux[1][1] =  (sigma+max(0.,transport_flux))*fv_sb_[0]->JxW(k);

                for (int n=0; n<2; n++)
                {
                    if (!own_element_id[n]) continue;

                    for (unsigned int i=0; i<n_dofs[n]; i++)
                        for (int m=0; m<2; m++)
                            for (unsigned int j=0; j<n_dofs[m]; j++)
                                local_matrix_[(i+n*n_dofs[0])*(n_dofs[0]+n_dofs[1]) + m*n_dofs[0] + j] +=
                                        comm_flux[m][n]*fv_sb_[m]->shape_value(j,k)*fv_sb_[n]->shape_value(i,k);
                }
                k++;
            }
            data_->ls[sbi]->mat_set_values(n_dofs[0]+n_dofs[1], &(side_dof_indices_vb_[0]), n_dofs[0]+n_dofs[1], &(side_dof_indices_vb_[0]), &(local_matrix_[0]));
        }
    }

    /// Implements @p AssemblyBase::reallocate_cache.
    void reallocate_cache(const ElementCacheMap &cache_map) override
    {
        data_->cache_reallocate(cache_map);
    }


private:
    shared_ptr<FiniteElement<dim>> fe_;         ///< Finite element for the solution of the advection-diffusion equation.
    shared_ptr<FiniteElement<dim-1>> fe_low_;   ///< Finite element for the solution of the advection-diffusion equation (dim-1).

    /// Data object shared with TransportDG
    EqDataDG *data_;

    unsigned int ndofs_;                                      ///< Number of dofs
    unsigned int qsize_lower_dim_;                            ///< Size of quadrature of dim-1
    FEValues<3> fe_values_;                                   ///< FEValues of object (of P disc finite element type)
    FEValues<3> fe_values_vb_;                                ///< FEValues of dim-1 object (of P disc finite element type)
    FEValues<3> fe_values_side_;                              ///< FEValues of object (of P disc finite element type)
    vector<FEValues<3>> fe_values_vec_;                       ///< Vector of FEValues of object (of P disc finite element types)
    vector<FEValues<3>*> fv_sb_;                              ///< Auxiliary vector, holds FEValues objects for assemble element-side

    vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
    vector< vector<LongIdx> > side_dof_indices_;              ///< Vector of vectors of side DOF indices
    vector<LongIdx> side_dof_indices_vb_;                     ///< Vector of side DOF indices (assemble element-side fluxex)
    vector<PetscScalar> local_matrix_;                        ///< Auxiliary vector for assemble methods

    template < template<IntDim...> class DimAssembly>
    friend class GenericAssembly;

};


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim, class Model>
class SourcesAssemblyDG : public AssemblyBase<dim>
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    SourcesAssemblyDG(EqDataDG *data)
    : AssemblyBase<dim>(data->dg_order), data_(data) {}

    /// Destructor.
    ~SourcesAssemblyDG() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(std::shared_ptr<Balance> balance) {
        this->balance_ = balance;

        fe_ = std::make_shared< FE_P_disc<dim> >(data_->dg_order);
        fe_values_.initialize(*this->quad_, *fe_, update_values | update_gradients | update_JxW_values | update_quadrature_points);
        ndofs_ = fe_->n_dofs();
        dof_indices_.resize(ndofs_);
        local_rhs_.resize(ndofs_);
        local_source_balance_vector_.resize(ndofs_);
        local_source_balance_rhs_.resize(ndofs_);
    }


    /// Assemble integral over element
    void assemble_volume_integrals(DHCellAccessor cell) override
    {
    	ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");

        ElementAccessor<3> elm = cell.elm();
        unsigned int k;
        double source;

        fe_values_.reinit(elm);
        cell.get_dof_indices(dof_indices_);

        // assemble the local stiffness matrix
        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            fill_n( &(local_rhs_[0]), ndofs_, 0 );
            local_source_balance_vector_.assign(ndofs_, 0);
            local_source_balance_rhs_.assign(ndofs_, 0);

            k=0;
            for (auto p : data_->sources_assembly_->bulk_points(dim, cell) )
            {
                source = (data_->sources_density_out[sbi](p) + data_->sources_conc_out[sbi](p)*data_->sources_sigma_out[sbi](p))*fe_values_.JxW(k);

                for (unsigned int i=0; i<ndofs_; i++)
                    local_rhs_[i] += source*fe_values_.shape_value(i,k);
                k++;
            }
            data_->ls[sbi]->rhs_set_values(ndofs_, &(dof_indices_[0]), &(local_rhs_[0]));

            for (unsigned int i=0; i<ndofs_; i++)
            {
                k=0;
                for (auto p : data_->sources_assembly_->bulk_points(dim, cell) )
                {
                    local_source_balance_vector_[i] -= data_->sources_sigma_out[sbi](p)*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                    k++;
                }

                local_source_balance_rhs_[i] += local_rhs_[i];
            }
            balance_->add_source_values(data_->subst_idx()[sbi], elm.region().bulk_idx(),
                                                 cell.get_loc_dof_indices(),
                                                 local_source_balance_vector_, local_source_balance_rhs_);
        }
    }

    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        balance_->start_source_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        balance_->finish_source_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::reallocate_cache.
    void reallocate_cache(const ElementCacheMap &cache_map) override
    {
        data_->cache_reallocate(cache_map);
    }


    private:
        shared_ptr<FiniteElement<dim>> fe_;         ///< Finite element for the solution of the advection-diffusion equation.

        /// Pointer to balance object
        std::shared_ptr<Balance> balance_;

        /// Data object shared with TransportDG
        EqDataDG *data_;

        unsigned int ndofs_;                                      ///< Number of dofs
        FEValues<3> fe_values_;                                   ///< FEValues of object (of P disc finite element type)

        vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
        vector<PetscScalar> local_rhs_;                           ///< Auxiliary vector for set_sources method.
        vector<PetscScalar> local_source_balance_vector_;         ///< Auxiliary vector for set_sources method.
        vector<PetscScalar> local_source_balance_rhs_;            ///< Auxiliary vector for set_sources method.

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};


/**
 * Assembles the r.h.s. components corresponding to the Dirichlet boundary conditions..
 */
template <unsigned int dim, class Model>
class BdrConditionAssemblyDG : public AssemblyBase<dim>
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    BdrConditionAssemblyDG(EqDataDG *data)
    : AssemblyBase<dim>(data->dg_order), data_(data) {}

    /// Destructor.
    ~BdrConditionAssemblyDG() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(std::shared_ptr<Balance> balance) {
        this->balance_ = balance;

        fe_ = std::make_shared< FE_P_disc<dim> >(data_->dg_order);
        fe_values_side_.initialize(*this->quad_low_, *fe_, update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points);
        ndofs_ = fe_->n_dofs();
        dof_indices_.resize(ndofs_);
        local_rhs_.resize(ndofs_);
        local_flux_balance_vector_.resize(ndofs_);
    }


    /// Implements @p AssemblyBase::assemble_fluxes_boundary.
    void assemble_fluxes_boundary(DHCellSide cell_side, const TimeStep &step) override
    {
        unsigned int k;

        const unsigned int cond_idx = cell_side.side().cond_idx();

        ElementAccessor<3> bc_elm = cell_side.cond().element_accessor();

        fe_values_side_.reinit(cell_side.side());

        const DHCellAccessor &cell = cell_side.cell();
        cell.get_dof_indices(dof_indices_);

        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            fill_n(&(local_rhs_[0]), ndofs_, 0);
            local_flux_balance_vector_.assign(ndofs_, 0);
            local_flux_balance_rhs_ = 0;

            double side_flux = 0;
            k=0;
            for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) ) {
                side_flux += arma::dot(data_->advection_coef[sbi](p), fe_values_side_.normal_vector(k))*fe_values_side_.JxW(k);
                k++;
            }
            double transport_flux = side_flux/cell_side.measure();

            auto p_side = *( data_->bdr_cond_assembly_->boundary_points(dim, cell_side)).begin();
            auto p_bdr = p_side.point_bdr(cell_side.cond().element_accessor() );
            unsigned int bc_type = data_->bc_type[sbi](p_bdr);
            if (bc_type == AdvectionDiffusionModel::abc_inflow && side_flux < 0)
            {
                k=0;
                for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    auto p_bdr = p.point_bdr(bc_elm);
                    double bc_term = -transport_flux*data_->bc_dirichlet_value[sbi](p_bdr)*fe_values_side_.JxW(k);
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_rhs_[i] += bc_term*fe_values_side_.shape_value(i,k);
                    k++;
                }
                for (unsigned int i=0; i<ndofs_; i++)
                    local_flux_balance_rhs_ -= local_rhs_[i];
            }
            else if (bc_type == AdvectionDiffusionModel::abc_dirichlet)
            {
                k=0;
                for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    auto p_bdr = p.point_bdr(bc_elm);
                    double bc_term = data_->gamma[sbi][cond_idx]*data_->bc_dirichlet_value[sbi](p_bdr)*fe_values_side_.JxW(k);
                    arma::vec3 bc_grad = -data_->bc_dirichlet_value[sbi](p_bdr)*fe_values_side_.JxW(k)*data_->dg_variant*(arma::trans(data_->diffusion_coef[sbi](p))*fe_values_side_.normal_vector(k));
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_rhs_[i] += bc_term*fe_values_side_.shape_value(i,k)
                                + arma::dot(bc_grad,fe_values_side_.shape_grad(i,k));
                    k++;
                }
                k=0;
                for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    for (unsigned int i=0; i<ndofs_; i++)
                    {
                        local_flux_balance_vector_[i] += (arma::dot(data_->advection_coef[sbi](p), fe_values_side_.normal_vector(k))*fe_values_side_.shape_value(i,k)
                                - arma::dot(data_->diffusion_coef[sbi](p)*fe_values_side_.shape_grad(i,k),fe_values_side_.normal_vector(k))
                                + data_->gamma[sbi][cond_idx]*fe_values_side_.shape_value(i,k))*fe_values_side_.JxW(k);
                    }
                    k++;
                }
                if (step.index() > 0)
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_flux_balance_rhs_ -= local_rhs_[i];
            }
            else if (bc_type == AdvectionDiffusionModel::abc_total_flux)
            {
            	k=0;
            	for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    auto p_bdr = p.point_bdr(bc_elm);
                    double bc_term = data_->cross_section(p) * (data_->bc_robin_sigma[sbi](p_bdr)*data_->bc_dirichlet_value[sbi](p_bdr) +
                            data_->bc_flux[sbi](p_bdr)) * fe_values_side_.JxW(k);
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_rhs_[i] += bc_term*fe_values_side_.shape_value(i,k);
                    k++;
                }

                for (unsigned int i=0; i<ndofs_; i++)
                {
                    k=0;
                    for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) ) {
                        auto p_bdr = p.point_bdr(bc_elm);
                        local_flux_balance_vector_[i] += data_->cross_section(p) * data_->bc_robin_sigma[sbi](p_bdr) *
                                fe_values_side_.JxW(k) * fe_values_side_.shape_value(i,k);
                        k++;
                	}
                    local_flux_balance_rhs_ -= local_rhs_[i];
                }
            }
            else if (bc_type == AdvectionDiffusionModel::abc_diffusive_flux)
            {
            	k=0;
            	for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    auto p_bdr = p.point_bdr(bc_elm);
                    double bc_term = data_->cross_section(p) * (data_->bc_robin_sigma[sbi](p_bdr)*data_->bc_dirichlet_value[sbi](p_bdr) +
                            data_->bc_flux[sbi](p_bdr)) * fe_values_side_.JxW(k);
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_rhs_[i] += bc_term*fe_values_side_.shape_value(i,k);
                    k++;
                }

                for (unsigned int i=0; i<ndofs_; i++)
                {
                    k=0;
                    for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) ) {
                        auto p_bdr = p.point_bdr(bc_elm);
                        local_flux_balance_vector_[i] += data_->cross_section(p)*(arma::dot(data_->advection_coef[sbi](p), fe_values_side_.normal_vector(k)) +
                                data_->bc_robin_sigma[sbi](p_bdr))*fe_values_side_.JxW(k)*fe_values_side_.shape_value(i,k);
                        k++;
                	}
                    local_flux_balance_rhs_ -= local_rhs_[i];
                }
            }
            else if (bc_type == AdvectionDiffusionModel::abc_inflow && side_flux >= 0)
            {
                k=0;
                for (auto p : data_->bdr_cond_assembly_->boundary_points(dim, cell_side) )
                {
                    for (unsigned int i=0; i<ndofs_; i++)
                        local_flux_balance_vector_[i] += arma::dot(data_->advection_coef[sbi](p), fe_values_side_.normal_vector(k))*fe_values_side_.JxW(k)*fe_values_side_.shape_value(i,k);
                    k++;
                }
            }
            data_->ls[sbi]->rhs_set_values(ndofs_, &(dof_indices_[0]), &(local_rhs_[0]));

            balance_->add_flux_values(data_->subst_idx()[sbi], cell_side,
                                          cell.get_loc_dof_indices(),
                                          local_flux_balance_vector_, local_flux_balance_rhs_);
        }
    }

    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        balance_->start_flux_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        balance_->finish_flux_assembly( data_->subst_idx() );
    }

    /// Implements @p AssemblyBase::reallocate_cache.
    void reallocate_cache(const ElementCacheMap &cache_map) override
    {
        data_->cache_reallocate(cache_map);
    }


    private:
        shared_ptr<FiniteElement<dim>> fe_;         ///< Finite element for the solution of the advection-diffusion equation.

        /// Pointer to balance object
        std::shared_ptr<Balance> balance_;

        /// Data object shared with TransportDG
        EqDataDG *data_;

        unsigned int ndofs_;                                      ///< Number of dofs
        FEValues<3> fe_values_side_;                              ///< FEValues of object (of P disc finite element type)

        vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
        vector<PetscScalar> local_rhs_;                           ///< Auxiliary vector for set_sources method.
        vector<PetscScalar> local_flux_balance_vector_;           ///< Auxiliary vector for set_boundary_conditions method.
        PetscScalar local_flux_balance_rhs_;                      ///< Auxiliary variable for set_boundary_conditions method.

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};


/**
 * Auxiliary container class sets the initial condition.
 */
template <unsigned int dim, class Model>
class InitConditionAssemblyDG : public AssemblyBase<dim>
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    InitConditionAssemblyDG(EqDataDG *data)
    : AssemblyBase<dim>(data->dg_order), data_(data) {}

    /// Destructor.
    ~InitConditionAssemblyDG() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(FMT_UNUSED std::shared_ptr<Balance> balance) {
        fe_ = std::make_shared< FE_P_disc<dim> >(data_->dg_order);
        fe_values_.initialize(*this->quad_, *fe_, update_values | update_gradients | update_JxW_values | update_quadrature_points);
        ndofs_ = fe_->n_dofs();
        dof_indices_.resize(ndofs_);
        local_matrix_.resize(4*ndofs_*ndofs_);
        local_rhs_.resize(ndofs_);
    }


    /// Assemble integral over element
    void assemble_volume_integrals(DHCellAccessor cell) override
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");

        unsigned int k;
        ElementAccessor<3> elem = cell.elm();
        cell.get_dof_indices(dof_indices_);
        fe_values_.reinit(elem);

        for (unsigned int sbi=0; sbi<data_->n_substances(); sbi++)
        {
            for (unsigned int i=0; i<ndofs_; i++)
            {
                local_rhs_[i] = 0;
                for (unsigned int j=0; j<ndofs_; j++)
                    local_matrix_[i*ndofs_+j] = 0;
            }

            k=0;
            for (auto p : data_->init_cond_assembly_->bulk_points(dim, cell) )
            {
                double rhs_term = data_->init_condition[sbi](p)*fe_values_.JxW(k);

                for (unsigned int i=0; i<ndofs_; i++)
                {
                    for (unsigned int j=0; j<ndofs_; j++)
                        local_matrix_[i*ndofs_+j] += fe_values_.shape_value(i,k)*fe_values_.shape_value(j,k)*fe_values_.JxW(k);

                    local_rhs_[i] += fe_values_.shape_value(i,k)*rhs_term;
                }
                k++;
            }
            data_->ls[sbi]->set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]), &(local_rhs_[0]));
        }
    }

    /// Implements @p AssemblyBase::reallocate_cache.
    void reallocate_cache(const ElementCacheMap &cache_map) override
    {
        data_->cache_reallocate(cache_map);
    }


    private:
        shared_ptr<FiniteElement<dim>> fe_;         ///< Finite element for the solution of the advection-diffusion equation.

        /// Data object shared with TransportDG
        EqDataDG *data_;

        unsigned int ndofs_;                                      ///< Number of dofs
        FEValues<3> fe_values_;                                   ///< FEValues of object (of P disc finite element type)

        vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
        vector<PetscScalar> local_matrix_;                        ///< Auxiliary vector for assemble methods
        vector<PetscScalar> local_rhs_;                           ///< Auxiliary vector for set_sources method.

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};



#endif /* ASSEMBLY_DG_HH_ */


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
 * @file    assembly_convection.hh
 * @brief
 */

#ifndef ASSEMBLY_CONVECTION_HH_
#define ASSEMBLY_CONVECTION_HH_

#include "coupling/generic_assembly.hh"
#include "coupling/assembly_base.hh"
#include "transport/transport.h"
#include "fem/fe_p.hh"
#include "fem/fe_values.hh"
#include "quadrature/quadrature_lib.hh"
#include "coupling/balance.hh"
#include "fields/field_value_cache.hh"


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim>
class MassAssemblyConvection : public AssemblyBase<dim>
{
public:
    typedef typename ConvectionTransport::EqFields EqFields;
    typedef typename ConvectionTransport::EqData EqData;

    static constexpr const char * name() { return "MassAssemblyConvection"; }

    /// Constructor.
    MassAssemblyConvection(EqFields *eq_fields, EqData *eq_data)
    : AssemblyBase<dim>(0), eq_fields_(eq_fields), eq_data_(eq_data) {
        this->active_integrals_ = ActiveIntegrals::bulk;
        this->used_fields_ += eq_fields_->cross_section;
        this->used_fields_ += eq_fields_->water_content;
    }

    /// Destructor.
    ~MassAssemblyConvection() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(ElementCacheMap *element_cache_map) {
        this->element_cache_map_ = element_cache_map;
    }


    /// Assemble integral over element
    inline void cell_integral(DHCellAccessor cell, unsigned int element_patch_idx)
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");

        ElementAccessor<3> elm = cell.elm();
        // we have currently zero order P_Disc FE
        ASSERT_DBG(cell.get_loc_dof_indices().size() == 1);
        IntIdx local_p0_dof = cell.get_loc_dof_indices()[0];

        auto p = *( this->bulk_points(element_patch_idx).begin() );
        for (unsigned int sbi=0; sbi<eq_data_->n_substances(); ++sbi)
            eq_data_->balance_->add_mass_values(eq_data_->subst_idx[sbi], cell, {local_p0_dof},
                    {eq_fields_->cross_section(p)*eq_fields_->water_content(p)*elm.measure()}, 0);

        VecSetValue(eq_data_->mass_diag, eq_data_->dh_->get_local_to_global_map()[local_p0_dof],
                eq_fields_->cross_section(p)*eq_fields_->water_content(p), INSERT_VALUES);
    }

    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        VecZeroEntries(eq_data_->mass_diag);
        eq_data_->balance_->start_mass_assembly(eq_data_->subst_idx);
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        eq_data_->balance_->finish_mass_assembly(eq_data_->subst_idx);

        VecAssemblyBegin(eq_data_->mass_diag);
        VecAssemblyEnd(eq_data_->mass_diag);

        eq_data_->is_mass_diag_changed = true;
    }

    private:
        shared_ptr<FiniteElement<dim>> fe_;                    ///< Finite element for the solution of the advection-diffusion equation.

        /// Data objects shared with TransportDG
        EqFields *eq_fields_;
        EqData *eq_data_;

        /// Sub field set contains fields used in calculation.
        FieldSet used_fields_;

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};



/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim>
class InitCondAssemblyConvection : public AssemblyBase<dim>
{
public:
    typedef typename ConvectionTransport::EqFields EqFields;
    typedef typename ConvectionTransport::EqData EqData;

    static constexpr const char * name() { return "InitCondAssemblyConvection"; }

    /// Constructor.
    InitCondAssemblyConvection(EqFields *eq_fields, EqData *eq_data)
    : AssemblyBase<dim>(0), eq_fields_(eq_fields), eq_data_(eq_data) {
        this->active_integrals_ = ActiveIntegrals::bulk;
        this->used_fields_ += eq_fields_->init_conc;
    }

    /// Destructor.
    ~InitCondAssemblyConvection() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(ElementCacheMap *element_cache_map) {
        this->element_cache_map_ = element_cache_map;
        for (unsigned int sbi=0; sbi<eq_data_->n_substances(); sbi++) {
            vecs_.push_back(eq_fields_->conc_mobile_fe[sbi]->vec());
        }
    }


    /// Assemble integral over element
    inline void cell_integral(DHCellAccessor cell, unsigned int element_patch_idx)
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");

		LongIdx index = cell.local_idx();
		auto p = *( this->bulk_points(element_patch_idx).begin() );

		for (unsigned int sbi=0; sbi<eq_data_->n_substances(); sbi++) {
			vecs_[sbi].set( index, eq_fields_->init_conc[sbi](p) );
		}
    }

private:
    shared_ptr<FiniteElement<dim>> fe_;                    ///< Finite element for the solution of the advection-diffusion equation.

    /// Data objects shared with TransportDG
    EqFields *eq_fields_;
    EqData *eq_data_;

    std::vector<VectorMPI> vecs_;                          ///< Set of data vectors of conc_mobile_fe objects.

    /// Sub field set contains fields used in calculation.
    FieldSet used_fields_;

    template < template<IntDim...> class DimAssembly>
    friend class GenericAssembly;

};


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim>
class ConcSourcesBdrAssemblyConvection : public AssemblyBase<dim>
{
public:
    typedef typename ConvectionTransport::EqFields EqFields;
    typedef typename ConvectionTransport::EqData EqData;

    static constexpr const char * name() { return "ConcSourcesBdrAssemblyConvection"; }

    /// Constructor.
    ConcSourcesBdrAssemblyConvection(EqFields *eq_fields, EqData *eq_data)
    : AssemblyBase<dim>(0), eq_fields_(eq_fields), eq_data_(eq_data) {
        this->active_integrals_ = (ActiveIntegrals::bulk | ActiveIntegrals::boundary);
        this->used_fields_ += eq_fields_->cross_section;
        this->used_fields_ += eq_fields_->sources_sigma;
        this->used_fields_ += eq_fields_->sources_density;
        this->used_fields_ += eq_fields_->sources_conc;
        this->used_fields_ += eq_fields_->flow_flux;
        this->used_fields_ += eq_fields_->bc_conc;
    }

    /// Destructor.
    ~ConcSourcesBdrAssemblyConvection() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(ElementCacheMap *element_cache_map) {
        this->element_cache_map_ = element_cache_map;

        fe_ = std::make_shared< FE_P_disc<dim> >(0);
        UpdateFlags u = update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points;
        fe_values_side_.initialize(*this->quad_low_, *fe_, u);
    }


    /// Assemble integral over element
    inline void cell_integral(DHCellAccessor cell, unsigned int element_patch_idx)
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");
        if (!eq_data_->sources_changed_) return;

        // read for all substances
        double max_cfl=0;
        double source, diag;
		ElementAccessor<3> elm = cell.elm();
		// we have currently zero order P_Disc FE
		ASSERT_DBG(cell.get_loc_dof_indices().size() == 1);
		IntIdx local_p0_dof = cell.get_loc_dof_indices()[0];

		auto p = *( this->bulk_points(element_patch_idx).begin() );
        for (unsigned int sbi = 0; sbi < eq_data_->n_substances(); sbi++)
        {
            source = eq_fields_->cross_section(p) * (eq_fields_->sources_density[sbi](p)
                 + eq_fields_->sources_sigma[sbi](p) * eq_fields_->sources_conc[sbi](p));
            // addition to RHS
            eq_data_->corr_vec[sbi].set(local_p0_dof, source);
            // addition to diagonal of the transport matrix
            diag = eq_fields_->sources_sigma[sbi](p) * eq_fields_->cross_section(p);
            eq_data_->tm_diag[sbi][local_p0_dof] = - diag;

            // compute maximal cfl condition over all substances
            max_cfl = std::max(max_cfl, fabs(diag));

            eq_data_->balance_->add_source_values(sbi, elm.region().bulk_idx(), {local_p0_dof},
                                        {- eq_fields_->sources_sigma[sbi](p) * elm.measure() * eq_fields_->cross_section(p)},
                                        {source * elm.measure()});
        }

        eq_data_->cfl_source_[local_p0_dof] = max_cfl;
    }

    /// Assembles the fluxes on the boundary.
    inline void boundary_side_integral(DHCellSide cell_side)
    {
        ASSERT_EQ_DBG(cell_side.dim(), dim).error("Dimension of element mismatch!");
        if (!cell_side.cell().is_own()) return;

		// we have currently zero order P_Disc FE
		ASSERT_DBG(cell_side.cell().get_loc_dof_indices().size() == 1);
		IntIdx local_p0_dof = cell_side.cell().get_loc_dof_indices()[0];
        LongIdx glob_p0_dof = eq_data_->dh_->get_local_to_global_map()[local_p0_dof];

        fe_values_side_.reinit(cell_side.side());

        unsigned int sbi;
        auto p_side = *( this->boundary_points(cell_side).begin() );
        auto p_bdr = p_side.point_bdr(cell_side.cond().element_accessor() );
        double flux = arma::dot(eq_fields_->flow_flux(p_side), fe_values_side_.normal_vector(0)) * fe_values_side_.JxW(0);
        if (flux < 0.0) {
            double aij = -(flux / cell_side.element().measure() );

            for (sbi=0; sbi<eq_data_->n_substances(); sbi++)
            {
                double value = eq_fields_->bc_conc[sbi](p_bdr);

                VecSetValue(eq_data_->bcvcorr[sbi], glob_p0_dof, value * aij, ADD_VALUES);

                // CAUTION: It seems that PETSc possibly optimize allocated space during assembly.
                // So we have to add also values that may be non-zero in future due to changing velocity field.
                eq_data_->balance_->add_flux_values(eq_data_->subst_idx[sbi], cell_side,
                                          {local_p0_dof}, {0.0}, flux*value);
            }
        } else {
            for (sbi=0; sbi<eq_data_->n_substances(); sbi++)
                VecSetValue(eq_data_->bcvcorr[sbi], glob_p0_dof, 0, ADD_VALUES);

            for (sbi=0; sbi<eq_data_->n_substances(); sbi++)
                eq_data_->balance_->add_flux_values(eq_data_->subst_idx[sbi], cell_side,
                                          {local_p0_dof}, {flux}, 0.0);
        }
    }

    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        eq_data_->sources_changed_ = ( (eq_fields_->sources_density.changed() )
                || (eq_fields_->sources_conc.changed() )
                || (eq_fields_->sources_sigma.changed() )
                || (eq_fields_->cross_section.changed()));

        //TODO: would it be possible to check the change in data for chosen substance? (may be in multifields?)

    	if (eq_data_->sources_changed_) eq_data_->balance_->start_source_assembly(eq_data_->subst_idx);
        // Assembly bcvcorr vector
        for(unsigned int sbi=0; sbi < eq_data_->n_substances(); sbi++) VecZeroEntries(eq_data_->bcvcorr[sbi]);

        eq_data_->balance_->start_flux_assembly(eq_data_->subst_idx);
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        eq_data_->balance_->finish_flux_assembly(eq_data_->subst_idx);
        if (eq_data_->sources_changed_) eq_data_->balance_->finish_source_assembly(eq_data_->subst_idx);

        for (unsigned int sbi=0; sbi<eq_data_->n_substances(); sbi++)  	VecAssemblyBegin(eq_data_->bcvcorr[sbi]);
        for (unsigned int sbi=0; sbi<eq_data_->n_substances(); sbi++)   VecAssemblyEnd(eq_data_->bcvcorr[sbi]);

        // we are calling set_boundary_conditions() after next_time() and
        // we are using data from t() before, so we need to set corresponding bc time
        eq_data_->transport_bc_time = eq_data_->time_->last_t();
    }

    private:
        shared_ptr<FiniteElement<dim>> fe_;                    ///< Finite element for the solution of the advection-diffusion equation.

        /// Data objects shared with ConvectionTransport
        EqFields *eq_fields_;
        EqData *eq_data_;

        /// Sub field set contains fields used in calculation.
        FieldSet used_fields_;

        FEValues<3> fe_values_side_;                           ///< FEValues of object (of P disc finite element type)

        template < template<IntDim...> class DimAssembly>
        friend class GenericAssembly;

};


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim>
class MatrixMpiAssemblyConvection : public AssemblyBase<dim>
{
public:
    typedef typename ConvectionTransport::EqFields EqFields;
    typedef typename ConvectionTransport::EqData EqData;

    static constexpr const char * name() { return "MatrixMpiAssemblyConvection"; }

    /// Constructor.
    MatrixMpiAssemblyConvection(EqFields *eq_fields, EqData *eq_data)
    : AssemblyBase<dim>(0), eq_fields_(eq_fields), eq_data_(eq_data) {
        this->active_integrals_ = ActiveIntegrals::edge | ActiveIntegrals::coupling;
        this->used_fields_ += eq_fields_->flow_flux;
    }

    /// Destructor.
    ~MatrixMpiAssemblyConvection() {}

    /// Initialize auxiliary vectors and other data members
    void initialize(ElementCacheMap *element_cache_map) {
        this->element_cache_map_ = element_cache_map;

        fe_ = std::make_shared< FE_P_disc<dim> >(0);
        UpdateFlags u = update_values | update_gradients | update_side_JxW_values | update_normal_vectors | update_quadrature_points;
        fe_values_side_.initialize(*this->quad_low_, *fe_, u);
        fe_values_vec_.resize(eq_data_->max_edg_sides);
        for (unsigned int sid=0; sid<eq_data_->max_edg_sides; sid++)
        {
            fe_values_vec_[sid].initialize(*this->quad_low_, *fe_, u);
        }

    }

    /// Assembles the fluxes between sides of elements of the same dimension.
    inline void edge_integral(RangeConvert<DHEdgeSide, DHCellSide> edge_side_range) {
        ASSERT_EQ_DBG(edge_side_range.begin()->element().dim(), dim).error("Dimension of element mismatch!");

        int sid=0, s1, s2;
        edg_flux = 0.0;
        for( DHCellSide edge_side : edge_side_range )
        {
            fe_values_vec_[sid].reinit(edge_side.side());
            auto p = *( this->edge_points(edge_side).begin() );
            flux = arma::dot(eq_fields_->flow_flux(p), fe_values_vec_[sid].normal_vector(0)) * fe_values_vec_[sid].JxW(0);
            if (flux > 0.0) {
                eq_data_->cfl_flow_[edge_side.cell().local_idx()] -= (flux / edge_side.element().measure() );
                edg_flux += flux;
            }
            ++sid;
        }

        s1=0;
        for( DHCellSide edge_side1 : edge_side_range )
        {
            s2=-1; // need increment at begin of loop (see conditionally 'continue' directions)
            auto p1 = *( this->edge_points(edge_side1).begin() );
            flux = arma::dot(eq_fields_->flow_flux(p1), fe_values_vec_[s1].normal_vector(0)) * fe_values_vec_[s1].JxW(0);
            for( DHCellSide edge_side2 : edge_side_range )
            {
                s2++;
                if (s2==s1) continue;

                auto p2 = *( this->edge_points(edge_side2).begin() );
                flux2 = arma::dot(eq_fields_->flow_flux(p2), fe_values_vec_[s2].normal_vector(0)) * fe_values_vec_[s2].JxW(0);
                new_i = eq_data_->row_4_el[edge_side1.element().idx()];
                new_j = eq_data_->row_4_el[edge_side2.element().idx()];
                if ( flux2 > 0.0 && flux <0.0)
                    aij = -(flux * flux2 / ( edg_flux * edge_side1.element().measure() ) );
                else aij =0;
                MatSetValue(eq_data_->tm, new_i, new_j, aij, INSERT_VALUES);
            }
            s1++;
        }
    }


    /// Assembles the fluxes between elements of different dimensions.
    inline void dimjoin_intergral(DHCellAccessor cell_lower_dim, DHCellSide neighb_side) {
        if (dim == 1) return;
        ASSERT_EQ_DBG(cell_lower_dim.dim(), dim-1).error("Dimension of element mismatch!");

        auto p_high = *( this->coupling_points(neighb_side).begin() );
        fe_values_side_.reinit(neighb_side.side());

        new_i = eq_data_->row_4_el[ cell_lower_dim.elm_idx() ];
        new_j = eq_data_->row_4_el[ neighb_side.elem_idx() ];
        flux = arma::dot(eq_fields_->flow_flux(p_high), fe_values_side_.normal_vector(0)) * fe_values_side_.JxW(0);

        // volume source - out-flow from higher dimension
        if (flux > 0.0)  aij = flux / cell_lower_dim.elm().measure();
        else aij=0;
        MatSetValue(eq_data_->tm, new_i, new_j, aij, INSERT_VALUES);
        // out flow from higher dim. already accounted

        // volume drain - in-flow to higher dimension
        if (flux < 0.0) {
            eq_data_->cfl_flow_[cell_lower_dim.local_idx()] -= (-flux) / cell_lower_dim.elm().measure();                           // diagonal drain
            aij = (-flux) / neighb_side.element().measure();
        } else aij=0;
        MatSetValue(eq_data_->tm, new_j, new_i, aij, INSERT_VALUES);
    }


    /// Implements @p AssemblyBase::begin.
    void begin() override
    {
        MatZeroEntries(eq_data_->tm);

        for (uint i=0; i<eq_data_->el_ds->lsize(); ++i)
            eq_data_->cfl_flow_[i] = 0.0;
    }

    /// Implements @p AssemblyBase::end.
    void end() override
    {
        LongIdx new_i;
        for ( DHCellAccessor dh_cell : eq_data_->dh_->own_range() ) {
            new_i = eq_data_->row_4_el[ dh_cell.elm_idx() ];
            MatSetValue(eq_data_->tm, new_i, new_i, eq_data_->cfl_flow_[dh_cell.local_idx()], INSERT_VALUES);

            eq_data_->cfl_flow_[dh_cell.local_idx()] = fabs(eq_data_->cfl_flow_[dh_cell.local_idx()]);
        }

        MatAssemblyBegin(eq_data_->tm, MAT_FINAL_ASSEMBLY);
        MatAssemblyEnd(eq_data_->tm, MAT_FINAL_ASSEMBLY);

        eq_data_->is_convection_matrix_scaled = false;
        eq_data_->transport_matrix_time = eq_data_->time_->t();
    }

private:
    shared_ptr<FiniteElement<dim>> fe_;                    ///< Finite element for the solution of the advection-diffusion equation.

    /// Data objects shared with ConvectionTransport
    EqFields *eq_fields_;
    EqData *eq_data_;

    /// Sub field set contains fields used in calculation.
    FieldSet used_fields_;

    FEValues<3> fe_values_side_;                           ///< FEValues of object (of P disc finite element type)
    vector<FEValues<3>> fe_values_vec_;                    ///< Vector of FEValues of object (of P disc finite element types)
    LongIdx new_i, new_j;
    double aij;
    double edg_flux, flux, flux2;

    template < template<IntDim...> class DimAssembly>
    friend class GenericAssembly;

};


#endif /* ASSEMBLY_CONVECTION_HH_ */

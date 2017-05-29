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
 * @file    fe_system.hh
 * @brief   Class FESystem for compound finite elements.
 * @author  Jan Stebel
 */

#ifndef FE_SYSTEM_HH_
#define FE_SYSTEM_HH_

#include <vector>
#include <memory>

// #include "system/global_defs.h"
// #include "system/system.hh"
#include "fem/finite_element.hh"
#include "fem/fe_values.hh"



/**
 * @brief Compound finite element on @p dim dimensional simplex.
 *
 * The finite element functions are continuous across the interfaces.
 */
template <unsigned int dim, unsigned int spacedim>
class FESystem : public FiniteElement<dim,spacedim>
{
    using FiniteElement<dim,spacedim>::number_of_dofs;
    using FiniteElement<dim,spacedim>::number_of_single_dofs;
    using FiniteElement<dim,spacedim>::number_of_pairs;
    using FiniteElement<dim,spacedim>::number_of_triples;
    using FiniteElement<dim,spacedim>::number_of_sextuples;
    using FiniteElement<dim,spacedim>::n_components_;
    using FiniteElement<dim,spacedim>::unit_support_points;
    using FiniteElement<dim,spacedim>::generalized_support_points;
    using FiniteElement<dim,spacedim>::order;

public:
    
    /// Constructor. FESystem created from vector of FE with given multiplicities.
    FESystem(FiniteElement<dim,spacedim> *fe, unsigned int n);

    /**
     * @brief Returns the @p ith basis function evaluated at the point @p p.
     * @param i Number of the basis function.
     * @param p Point of evaluation.
     */
    double basis_value(const unsigned int i, const arma::vec::fixed<dim> &p) const override;

    /**
     * @brief Returns the gradient of the @p ith basis function at the point @p p.
     * @param i Number of the basis function.
     * @param p Point of evaluation.
     */
    arma::vec::fixed<dim> basis_grad(const unsigned int i, const arma::vec::fixed<dim> &p) const override;

    /**
     * @brief The vector variant of basis_value must be implemented but may not be used.
     */
    double basis_value_component(const unsigned int i, 
                                                const arma::vec::fixed<dim> &p, 
                                                const unsigned int comp) const override;

    /**
     * @brief The vector variant of basis_grad must be implemented but may not be used.
     */
    arma::vec::fixed<dim> basis_grad_component(const unsigned int i, 
                                               const arma::vec::fixed<dim> &p, 
                                               const unsigned int comp) const override;
                                               
    UpdateFlags update_each(UpdateFlags flags) override;
    
    void compute_node_matrix() override;
    
    FEInternalData *initialize(const Quadrature<dim> &q, UpdateFlags flags) override;
    
    void fill_fe_values(
        const Quadrature<dim> &q,
        FEInternalData &data,
        FEValuesData<dim,spacedim> &fv_data) override;

    virtual ~FESystem();

private:
  
  struct DofComponentData {
    
    DofComponentData(unsigned int fei, unsigned int bi, unsigned co)
      : fe_index(fei),
        basis_index(bi),
        component_offset(co)
    {};
    
    unsigned int fe_index;
    unsigned int basis_index;
    unsigned int component_offset;
  };
  
  /// FESystem created from vector of FE.
  void initialize();
  

  std::vector<std::shared_ptr<FiniteElement<dim,spacedim> > > fe_;
  std::vector<DofComponentData> fe_dof_indices_;
  std::vector<unsigned int> dof_basis_;
  
};






#endif /* FE_SYSTEM_HH_ */

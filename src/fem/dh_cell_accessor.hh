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
 * @file    dh_cell_accessor.hh
 * @brief
 * @author  David Flanderka
 */

#ifndef DH_CELL_ACCESSOR_HH_
#define DH_CELL_ACCESSOR_HH_

#include "mesh/accessors.hh"
#include "fem/dofhandler.hh"

/**
 * Cell accessor allow iterate over DOF handler cells.
 *
 * Iterating is possible over different ranges of local and ghost elements.
 */
class DHCellAccessor {
public:
    /**
     * Default invalid accessor.
     */
	DHCellAccessor()
    : dof_handler_(NULL)
    {}

    /**
     * DOF cell accessor.
     */
	DHCellAccessor(const DOFHandlerMultiDim *dof_handler, unsigned int loc_idx)
    : dof_handler_(dof_handler), loc_ele_idx_(loc_idx)
    {}

    /// Return local index to element (index of DOF handler).
    inline unsigned int local_idx() const {
    	ASSERT_LT_DBG(loc_ele_idx_, dof_handler_->el_ds_->lsize()).error("Method 'local_idx()' can't be used for ghost cells!\n");
        return loc_ele_idx_;
    }

    /// Return serial idx to element of loc_ele_idx_.
    inline unsigned int element_idx() const {
    	unsigned int ds_lsize = dof_handler_->el_ds_->lsize();
        if (loc_ele_idx_<ds_lsize) return dof_handler_->el_index(loc_ele_idx_); //own elements
        else return dof_handler_->ghost_4_loc[loc_ele_idx_-ds_lsize]; //ghost elements
    }

    /// Return ElementAccessor to element of loc_ele_idx_.
    inline const ElementAccessor<3> element_accessor() const {
    	return dof_handler_->mesh()->element_accessor( element_idx() );
    }

    /**
     * @brief Fill vector of the global indices of dofs associated to the cell.
     *
     * @param indices Vector of dof indices on the cell.
     */
    unsigned int get_dof_indices(std::vector<int> &indices) const;

    /**
     * @brief Returns the indices of dofs associated to the cell on the local process.
     *
     * @param indices Array of dof indices on the cell.
     */
    unsigned int get_loc_dof_indices(std::vector<LongIdx> &indices) const;

    /// Return number of dofs on given cell.
    unsigned int n_dofs() const;

    /**
     * @brief Return dof on a given cell.
     * @param idof Number of dof on the cell.
     */
    const Dof &cell_dof(unsigned int idof) const;

    /// Iterates to next local element.
    inline void inc() {
        loc_ele_idx_++;
    }

    /// Comparison of accessors.
    bool operator==(const DHCellAccessor& other) {
    	return (loc_ele_idx_ == other.loc_ele_idx_);
    }

    /**
     * -> dereference operator
     *
     * Return ElementAccessor to element of loc_ele_idx_. Allow to simplify code:
 @code
     DHCellAccessor dh_ac(dh, loc_index);
     unsigned int dim;
     dim = dh_ac.element_accessor().dim();  // full format of access to element
     dim = dh_ac->dim();                    // short format with dereference operator
 @endcode
     */
    inline const ElementAccessor<3> operator ->() const {
    	return dof_handler_->mesh()->element_accessor( element_idx() );
    }

private:
    /// Pointer to the DOF handler owning the element.
    const DOFHandlerMultiDim * dof_handler_;
    /// Index into DOFHandler::el_4_loc array.
    unsigned int loc_ele_idx_;
};


inline unsigned int DHCellAccessor::get_dof_indices(std::vector<int> &indices) const
{
  unsigned int elem_idx = this->element_idx();
  unsigned int ndofs = 0;
  if ( dof_handler_->cell_starts_seq.size() > 0 && dof_handler_->dof_indices_seq.size() > 0)
  {
    ndofs = dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]+1]-dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]];
    for (unsigned int k=0; k<ndofs; k++)
      indices[k] = dof_handler_->dof_indices_seq[dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]]+k];
  }
  else
  {
    ndofs = dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]+1]-dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]];
    for (unsigned int k=0; k<ndofs; k++)
      indices[k] = dof_handler_->dof_indices[dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]]+k];
  }

  return ndofs;
}


inline unsigned int DHCellAccessor::get_loc_dof_indices(std::vector<LongIdx> &indices) const
{
  unsigned int elem_idx = this->element_idx();
  unsigned int ndofs = 0;
  if ( dof_handler_->cell_starts_seq.size() > 0 && dof_handler_->dof_indices_seq.size() > 0)
  {
    ndofs = dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]+1]-dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]];
    for (unsigned int k=0; k<ndofs; k++)
      indices[k] = dof_handler_->cell_starts_seq[dof_handler_->row_4_el[elem_idx]]+k;
  }
  else
  {
    ndofs = dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]+1]-dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]];
    for (unsigned int k=0; k<ndofs; k++)
      indices[k] = dof_handler_->cell_starts[dof_handler_->row_4_el[elem_idx]]+k;
  }

  return ndofs;
}


inline unsigned int DHCellAccessor::n_dofs() const
{
    switch (element_accessor().dim()) {
        case 1:
            return dof_handler_->fe<1>(*this)->n_dofs();
            break;
        case 2:
            return dof_handler_->fe<2>(*this)->n_dofs();
            break;
        case 3:
            return dof_handler_->fe<3>(*this)->n_dofs();
            break;
    }
}


inline const Dof &DHCellAccessor::cell_dof(unsigned int idof) const
{
    switch (element_accessor().dim())
    {
        case 1:
            return dof_handler_->fe<1>(*this)->dof(idof);
            break;
        case 2:
            return dof_handler_->fe<2>(*this)->dof(idof);
            break;
        case 3:
            return dof_handler_->fe<3>(*this)->dof(idof);
            break;
    }
}


#endif /* DH_CELL_ACCESSOR_HH_ */

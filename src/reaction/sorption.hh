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
 * @file    sorption.hh
 * @brief   This file contains classes representing sorption model.
 *          Sorption model can be computed both in case the dual porosity is considered or not.
 * 
 * The difference is only in the isotherm_reinit method. 
 * Passing immobile porosity from dual porosity model is solved in abstract class SorptionDual.
 * 
 * @todo
 * It seems that the methods isotherm_reinit() are different only at computation of scale_aqua and scale_sorbed.
 * So it could be moved to SorptionDual and the only method which would be virtual would be 
 * compute_sorbing_scale(). It is prepared in comment code.
 */

#ifndef SORPTION_H
#define SORPTION_H


#include <string>                     // for string
#include <vector>                     // for vector
#include "fields/field.hh"            // for Field
#include "fields/field_values.hh"     // for FieldValue<>::Scalar, FieldValue
#include "input/type_base.hh"         // for Array
#include "input/type_generic.hh"      // for Instance
#include "reaction/reaction_term.hh"  // for ReactionTerm
#include "reaction/sorption_base.hh"

class Mesh;
class Isotherm;
namespace Input {
	class Record;
	namespace Type { class Record; }
}
template <int spacedim> class ElementAccessor;


/** @brief Simple sorption model without dual porosity.
 * 
 */
class SorptionSimple:  public SorptionBase
{
public:
	typedef ReactionTerm FactoryBaseType;

    static const Input::Type::Record & get_input_type();

    /// Constructor.
    SorptionSimple(Mesh &init_mesh, Input::Record in_rec);
    
    /// Destructor.
    ~SorptionSimple(void);
  
protected:
    /// Computes @p CommonElementData.
    void compute_common_ele_data(const ElementAccessor<3> &elem) override;

private:
    /// Registrar of class to factory
    static const int registrar;
};


/** @brief Abstract class of sorption model in case dual porosity is considered.
 * 
 */
class SorptionDual:  public SorptionBase
{
public:
    /// Constructor.
    SorptionDual(Mesh &init_mesh, Input::Record in_rec,
                const string &output_conc_name,
                const string &output_conc_desc);

    /// Destructor.
    ~SorptionDual(void);
    
    /// Sets the immobile porosity field.
    inline void set_porosity_immobile(Field<3, FieldValue<3>::Scalar > &por_imm)
      { 
        immob_porosity_.copy_from(por_imm); 
      }

protected:
    /// Computes @p CommonElementData. Pure virtual.
    virtual void compute_common_ele_data(const ElementAccessor<3> &elem) = 0;
    
    Field<3, FieldValue<3>::Scalar > immob_porosity_; //< Immobile porosity field copied from transport
};


/** @brief Sorption model in mobile zone in case dual porosity is considered.
 * 
 */
class SorptionMob:  public SorptionDual
{
public:
	typedef ReactionTerm FactoryBaseType;

    static const Input::Type::Record & get_input_type();

    /// Constructor.
    SorptionMob(Mesh &init_mesh, Input::Record in_rec);
    
    /// Destructor.
    ~SorptionMob(void);
  
protected:
    /// Computes @p CommonElementData.
    void compute_common_ele_data(const ElementAccessor<3> &elem) override;

private:
    /// Registrar of class to factory
    static const int registrar;
};


/** @brief Sorption model in immobile zone in case dual porosity is considered.
 * 
 */
class SorptionImmob:  public SorptionDual
{
public:
	typedef ReactionTerm FactoryBaseType;

    static const Input::Type::Record & get_input_type();

    /// Constructor.
    SorptionImmob(Mesh &init_mesh, Input::Record in_rec);
    
    /// Destructor.
    ~SorptionImmob(void);

protected:
    /// Computes @p CommonElementData.
    void compute_common_ele_data(const ElementAccessor<3> &elem) override;

private:
    /// Registrar of class to factory
    static const int registrar;
};


#endif  // SORPTION_H

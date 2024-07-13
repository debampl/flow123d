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
 * @file    field_fe.hh
 * @brief   
 */

#ifndef FIELD_FE_HH_
#define FIELD_FE_HH_

#include "petscmat.h"
#include "system/system.hh"
#include "system/index_types.hh"
#include "fields/field_algo_base.hh"
#include "fields/field.hh"
#include "la/vector_mpi.hh"
#include "mesh/mesh.h"
#include "mesh/point.hh"
#include "mesh/bih_tree.hh"
#include "mesh/range_wrapper.hh"
#include "io/element_data_cache.hh"
#include "io/msh_basereader.hh"
#include "fem/fe_p.hh"
#include "fem/fe_system.hh"
#include "fem/dofhandler.hh"
#include "fem/finite_element.hh"
#include "fem/dh_cell_accessor.hh"
#include "fem/mapping_p1.hh"
#include "input/factory.hh"

#include <memory>



/**
 * Class representing fields given by finite element approximation.
 *
 */
template <int spacedim, class Value>
class FieldFE : public FieldAlgorithmBase<spacedim, Value>
{
public:
    typedef typename FieldAlgorithmBase<spacedim, Value>::Point Point;
    typedef FieldAlgorithmBase<spacedim, Value> FactoryBaseType;
	typedef typename Field<spacedim, Value>::FactoryBase FieldFactoryBaseType;

	/**
	 * Possible interpolations of input data.
	 */
	enum DataInterpolation
	{
		identic_msh,    //!< identical mesh
		equivalent_msh, //!< equivalent mesh (default value)
		gauss_p0,       //!< P0 interpolation (with the use of Gaussian distribution)
		interp_p0       //!< P0 interpolation (with the use of calculation of intersections)
	};

    /// Declaration of exception.
    TYPEDEF_ERR_INFO( EI_Field, std::string);
    TYPEDEF_ERR_INFO( EI_File, std::string);
    TYPEDEF_ERR_INFO( EI_ElemIdx, unsigned int);
    TYPEDEF_ERR_INFO( EI_Region, std::string);
    DECLARE_EXCEPTION( ExcInvalidElemeDim, << "Dimension of element in target mesh must be 0, 1 or 2! elm.idx() = " << EI_ElemIdx::val << ".\n" );
    DECLARE_INPUT_EXCEPTION( ExcUndefElementValue,
            << "FieldFE " << EI_Field::qval << " on region " << EI_Region::qval << " have invalid value .\n"
            << "Provided by file " << EI_File::qval << " at element ID " << EI_ElemIdx::val << ".\n"
            << "Please specify in default_value key.\n");



    static const unsigned int undef_uint = -1;

    /**
     * Default constructor, optionally we need number of components @p n_comp in the case of Vector valued fields.
     */
    FieldFE(unsigned int n_comp=0);

    /**
     * Return Record for initialization of FieldFE that is derived from Abstract
     */
    static const Input::Type::Record & get_input_type();

    /**
     * Return Input selection for discretization type (determines the section of VTK file).
     */
    static const Input::Type::Selection & get_disc_selection_input_type();

    /**
     * Return Input selection that allow to set interpolation of input data.
     */
    static const Input::Type::Selection & get_interp_selection_input_type();

    /**
     * Factory class (descendant of @p Field<...>::FactoryBase) that is necessary
     * for setting pressure values are piezometric head values.
     */
    class NativeFactory : public FieldFactoryBaseType {
    public:
        /// Constructor.
        NativeFactory(unsigned int index, std::shared_ptr<DOFHandlerMultiDim> conc_dof_handler, VectorMPI dof_vector = VectorMPI::sequential(0))
        : index_(index),
          conc_dof_handler_(conc_dof_handler),
          dof_vector_(dof_vector)
        {}

    	typename Field<spacedim,Value>::FieldBasePtr create_field(Input::Record rec, const FieldCommon &field) override;

        unsigned int index_;
        std::shared_ptr<DOFHandlerMultiDim> conc_dof_handler_;
        VectorMPI dof_vector_;
    };


    /**
     * Setter for the finite element data.
     * @param dh              Dof handler.
     * @param dof_values      Vector of dof values, optional (create own vector according to dofhandler).
     * @param block_index     Index of block (in FESystem) or '-1' for FieldFE under simple DOF handler.
     * @return                Data vector of dof values.
     */
    VectorMPI set_fe_data(std::shared_ptr<DOFHandlerMultiDim> dh, VectorMPI dof_values = VectorMPI::sequential(0),
            unsigned int block_index = FieldFE<spacedim, Value>::undef_uint);

    /**
     * Overload @p FieldAlgorithmBase::cache_update
     */
    void cache_update(FieldValueCache<typename Value::element_type> &data_cache,
			ElementCacheMap &cache_map, unsigned int region_patch_idx) override;

    /**
     * Overload @p FieldAlgorithmBase::cache_reinit
     *
     * Reinit fe_values_ data member.
     */
    void cache_reinit(const ElementCacheMap &cache_map) override;

	/**
	 * Initialization from the input interface.
	 */
	virtual void init_from_input(const Input::Record &rec, const struct FieldAlgoBaseInitData& init_data);


    /**
     * Update time and possibly update data from GMSH file.
     */
    bool set_time(const TimeStep &time) override;


    /**
     * Set target mesh.
     */
    void set_mesh(const Mesh *mesh) override;


    /**
     * Copy data vector to given output ElementDataCache
     */
    void native_data_to_cache(ElementDataCache<double> &output_data_cache);


    /**
     * Return size of vector of data stored in Field
     */
    unsigned int data_size() const;

    inline std::shared_ptr<DOFHandlerMultiDim> get_dofhandler() const {
    	return dh_;
    }

    inline VectorMPI& vec() {
    	return data_vec_;
    }

    inline const VectorMPI& vec() const {
    	return data_vec_;
    }

    /// Call begin scatter functions (local to ghost) on data vector
    void local_to_ghost_data_scatter_begin();

    /// Call end scatter functions (local to ghost) on data vector
    void local_to_ghost_data_scatter_end();

    /// Destructor.
	virtual ~FieldFE();

private:
	/**
	 * Helper class holds specific data of field evaluation.
	 */
    class FEItem {
    public:
        unsigned int comp_index_;
        unsigned int range_begin_;
        unsigned int range_end_;
    };

	/**
	 * Helper class holds data of invalid values of all regions.
	 *
	 * If region contains invalid element value (typically 'not a number') is_invalid_ flag is set to true
	 * and other information (element id an value) are stored. Check of invalid values is performed
	 * during processing data of reader cache and possible exception is thrown only if FieldFE is defined
	 * on appropriate region.
	 *
	 * Data of all regions are stored in vector of RegionValueErr instances.
	 */
    class RegionValueErr {
    public:
        /// Default constructor, sets valid region
        RegionValueErr() : is_invalid_(false) {}

        /// Constructor, sets invalid region, element and value specification
        RegionValueErr(const std::string &region_name, unsigned int elm_id, double value) {
            is_invalid_ = true;
            region_name_ = region_name;
            elm_id_ = elm_id;
            value_ = value;
        }

        bool is_invalid_;
        std::string region_name_;
        unsigned int elm_id_;
        double value_;
    };

	/// Create DofHandler object
	void make_dof_handler(const MeshBase *mesh);

	/// Interpolate data (use Gaussian distribution) over all elements of target mesh.
	void interpolate_gauss();

	/// Interpolate data (use intersection library) over all elements of target mesh.
	void interpolate_intersection();

//	/// Calculate native data over all elements of target mesh.
//	void calculate_native_values(ElementDataCache<double>::CacheData data_cache);
//
//	/// Calculate data of equivalent_mesh interpolation on input over all elements of target mesh.
//	void calculate_equivalent_values(ElementDataCache<double>::CacheData data_cache);

	/// Calculate data of equivalent_mesh interpolation or native data on input over all elements of target mesh.
	void calculate_element_values();

	/// Initialize FEValues object of given dimension.
	template <unsigned int dim>
	Quadrature init_quad(std::shared_ptr<EvalPoints> eval_points);

    inline Armor::ArmaMat<typename Value::element_type, Value::NRows_, Value::NCols_> handle_fe_shape(unsigned int dim,
            unsigned int i_dof, unsigned int i_qp)
    {
        Armor::ArmaMat<typename Value::element_type, Value::NCols_, Value::NRows_> v;
        for (unsigned int c=0; c<Value::NRows_*Value::NCols_; ++c)
            v(c/spacedim,c%spacedim) = fe_values_[dim].shape_value_component(i_dof, i_qp, c);
        if (Value::NRows_ == Value::NCols_)
            return v;
        else
            return v.t();
    }

    template<unsigned int dim>
    void fill_fe_system_data(unsigned int block_index) {
        auto fe_system_ptr = std::dynamic_pointer_cast<FESystem<dim>>( dh_->ds()->fe()[Dim<dim>{}] );
        ASSERT(fe_system_ptr != nullptr).error("Wrong type, must be FESystem!\n");
        this->fe_item_[dim].comp_index_ = fe_system_ptr->function_space()->dof_indices()[block_index].component_offset;
        this->fe_item_[dim].range_begin_ = fe_system_ptr->fe_dofs(block_index)[0];
        this->fe_item_[dim].range_end_ = this->fe_item_[dim].range_begin_ + fe_system_ptr->fe()[block_index]->n_dofs();
    }


    template<unsigned int dim>
    void fill_fe_item() {
        this->fe_item_[dim].comp_index_ = 0;
        this->fe_item_[dim].range_begin_ = 0;
        this->fe_item_[dim].range_end_ = dh_->ds()->fe()[Dim<dim>{}]->n_dofs();
    }

    /**
     * Method computes value of given input cache element.
     *
     * If computed value is invalid (e.g. NaN value) sets the data specifying error value.
     * @param i_cache_el Index of element of input ElementDataCache
     * @param elm_idx Idx of element of computational mesh
     * @param region_name Region of computational mesh
     * @param actual_compute_region_error Data object holding data of region with invalid value.
     */
    double get_scaled_value(int i_cache_el, unsigned int elm_idx, const std::string &region_name, RegionValueErr &actual_compute_region_error);

    /**
     * Helper method. Compute real coordinates and weights (use QGauss) of given element.
     *
     * Method is needs in Gauss interpolation.
     */
    template<int elemdim>
    unsigned int compute_fe_quadrature(std::vector<arma::vec::fixed<3>> & q_points, std::vector<double> & q_weights,
    		const ElementAccessor<spacedim> &elm, unsigned int order=3)
    {
        static_assert(elemdim <= spacedim, "Dimension of element must be less equal than spacedim.");
    	static const double weight_coefs[] = { 1., 1., 2., 6. };

    	QGauss qgauss(elemdim, order);
    	arma::mat map_mat = MappingP1<elemdim,spacedim>::element_map(elm);

    	for(unsigned i=0; i<qgauss.size(); ++i) {
    		q_weights[i] = qgauss.weight(i)*weight_coefs[elemdim];
    		q_points[i] = MappingP1<elemdim,spacedim>::project_unit_to_real(RefElement<elemdim>::local_to_bary(qgauss.point<elemdim>(i)), map_mat);
    	}

    	return qgauss.size();
    }


    /// DOF handler object
    std::shared_ptr<DOFHandlerMultiDim> dh_;
    /// Store data of Field
    VectorMPI data_vec_;

	/// mesh reader file
	FilePath reader_file_;

	/// field name read from input
	std::string field_name_;

	/// Specify section where to find the field data in input mesh file
	OutputTime::DiscreteSpace discretization_;

	/// Specify type of FE data interpolation
	DataInterpolation interpolation_;

	/// Field flags.
	FieldFlag::Flags flags_;

    /// Default value of element if not set in mesh data file
    double default_value_;

    /// Accessor to Input::Record
    Input::Record in_rec_;

    /// Is set in set_mesh method. Value true means, that we accept only boundary element accessors in the @p value method.
    bool boundary_domain_;

    /// List of FEValues objects of dimensions 0,1,2,3 used for value calculation
    std::vector<FEValues<spacedim>> fe_values_;

    /// Maps element indices from computational mesh to the  source (data).
    std::shared_ptr<EquivalentMeshMap> source_target_mesh_elm_map_;

    /// Holds specific data of field evaluation over all dimensions.
    std::array<FEItem, 4> fe_item_;
    MixedPtr<FiniteElement> fe_;

    /// Set holds data of valid / invalid element values on all regions
    std::vector<RegionValueErr> region_value_err_;

    /// Input ElementDataCache is stored in set_time and used in all evaluation and interpolation methods.
    ElementDataCache<double>::CacheData input_data_cache_;

    /// Registrar of class to factory
    static const int registrar;
};


/** Create FieldFE from dof handler */
template <int spacedim, class Value>
std::shared_ptr<FieldFE<spacedim, Value> > create_field_fe(std::shared_ptr<DOFHandlerMultiDim> dh,
                                                           VectorMPI *vec = nullptr,
														   unsigned int block_index = FieldFE<spacedim, Value>::undef_uint)
{
	// Construct FieldFE
	std::shared_ptr< FieldFE<spacedim, Value> > field_ptr = std::make_shared< FieldFE<spacedim, Value> >();
    if (vec == nullptr)
	    field_ptr->set_fe_data( dh, dh->create_vector(), block_index );
    else
        field_ptr->set_fe_data( dh, *vec, block_index );

	return field_ptr;
}


/** Create FieldFE with parallel VectorMPI from finite element */
template <int spacedim, class Value>
std::shared_ptr<FieldFE<spacedim, Value> > create_field_fe(Mesh & mesh, const MixedPtr<FiniteElement> &fe)
{
	// Prepare DOF handler
	std::shared_ptr<DOFHandlerMultiDim> dh_par = std::make_shared<DOFHandlerMultiDim>(mesh);
	std::shared_ptr<DiscreteSpace> ds = std::make_shared<EqualOrderDiscreteSpace>( &mesh, fe);
	dh_par->distribute_dofs(ds);

	return create_field_fe<spacedim,Value>(dh_par);
}


#endif /* FIELD_FE_HH_ */

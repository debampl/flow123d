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
 * @file    output_time.hh
 * @brief   
 */

#ifndef OUTPUT_TIME_HH_
#define OUTPUT_TIME_HH_

#include <vector>
#include <string>
#include <fstream>
#include "input/accessors.hh"
#include "io/element_data_cache.hh"

class FilePath;
class Observe;
class ElementDataCacheBase;
class Mesh;
class TimeGovernor;
class OutputMeshBase;
class OutputMesh;
class OutputMeshDiscontinuous;


/**
 * \brief The class for outputting data during time.
 *
 * This class is descendant of Output class. This class is used for outputting
 * data varying in time. Own output to specific file formats is done at other
 * places to. See output_vtk.cc and output_msh.cc.
 */
class OutputTime {

public:
    /// Default constructor. Only for testing.
    OutputTime();


    /**
     * \brief Constructor of OutputTime object. It opens base file for writing.
     *
     * \param[in] equation_name The name of equation, used for forming output file name.
     * \param[in] in_rec The reference on the input record
     */
    virtual void init_from_input(const std::string &equation_name, const Input::Record &in_rec);

    /**
     * \brief Destructor of OutputTime. It doesn't do anything, because all
     * necessary destructors will be called in destructor of Output
     */
    virtual ~OutputTime();

    /**
     * Return the input array for the output time set of the output stream.
     */
    Input::Iterator<Input::Array> get_time_set_array();

    /**
     * Return the input record for the output mesh of the output stream.
     */
    Input::Iterator<Input::Record> get_output_mesh_record();

    /**
     * \brief The specification of output stream
     *
     * \return This variable defines record for output stream
     */
    static const Input::Type::Record & get_input_type();

    /**
     * \brief The specification of output file format
     */
    static Input::Type::Abstract & get_input_format_type();

    /**
     * Types of reference data
     *
     * NATIVE_DATA represents output of FieldFE in our own format, Paraview ignores this format.
     */
    static const unsigned int N_DISCRETE_SPACES = 4;
    enum DiscreteSpace {
        NODE_DATA       = 0,
        CORNER_DATA     = 1,
        ELEM_DATA       = 2,
        NATIVE_DATA     = 3,
		MESH_DEFINITION = 9,
		UNDEFINED       = 10
    };

    /**
     * Maps names of output fields required by user to their indices in
     * output_data_vec_.
     */
    typedef unsigned int DiscreteSpaceFlags;

    /**
     * Map field name to its OutputData object.
     */
    typedef std::shared_ptr<ElementDataCacheBase> OutputDataPtr;
    typedef std::vector< OutputDataPtr > OutputDataFieldVec;

    /// pair of field name and shape (= Scalar 1, Vector 3, Tensor 9)
    typedef std::pair< std::string, unsigned int > FieldInterpolationData;
    typedef std::map< DiscreteSpace, std::vector<FieldInterpolationData> > InterpolationMap;

    /**
     * \brief This method delete all object instances of class OutputTime stored
     * in output_streams vector
     */
    //static void destroy_all(void);

    /**
     * \brief This method tries to create new instance of OutputTime according
     * record in configuration file.
     */
    static std::shared_ptr<OutputTime> create_output_stream(const std::string &equation_name, const Input::Record &in_rec);
    
    /**
     * Write all data registered as a new time frame.
     */
    void write_time_frame();

    /**
     * Getter of the observe object.
     */
    std::shared_ptr<Observe> observe(Mesh *mesh);

    /**
     * \brief Clear data for output computed by method @p compute_field_data.
     */
    void clear_data(void);

    /**
     * Return if shared pointer to output_mesh_ is created.
     */
    inline bool is_output_mesh_init() {
    	return (bool)(output_mesh_);
    }

    /// Return auxiliary flag enable_refinement_.
    inline bool enable_refinement() {
        return enable_refinement_;
    }

    /**
     * Set shared pointer of \p output_mesh_.
     */
    void set_output_mesh_ptr(std::shared_ptr<OutputMeshBase> mesh_ptr);

    /**
     * Get shared pointer of \p output_mesh_.
     */
    std::shared_ptr<OutputMeshBase> get_output_mesh_ptr();

    /**
     * Return MPI rank of process
     */
    inline int get_rank() {
    	return rank;
    }

    /**
     * Update the last time is actual \p time is less than \p field_time
     */
    void update_time(double field_time);

    /**
     * Prepare data for computing field values.
     *
     * Method:
     *  - compute discontinuous mesh if CORNER_DATA is calculated
     *  - find and return ElementDataCache of given field_name, create its if doesn't exist
     *
     * @param field_name Quantity name of founding ElementDataCache
     * @param space_type Output discrete space
     * @param n_rows     Count of rows of data cache (used only if new cache is created)
     * @param n_cols     Count of columns of data cache (used only if new cache is created)
     * @param size       Size of data cache (used only if new cache is created and only for native data)
     */
    template <typename T>
    ElementDataCache<T> & prepare_compute_data(std::string field_name, DiscreteSpace space_type, unsigned int n_rows, unsigned int n_cols);

    /**
     * Return if file format is in parallel / serial version.
     */
    inline bool is_parallel() {
    	return parallel_;
    }

    /// Complete information about dummy fields, method has effect only for GMSH output.
    virtual void add_dummy_fields();


protected:
    
    /**
     * Change main filename to have prescribed extension.
     */
    void fix_main_file_extension(std::string extension);


    /**
     * \brief Return unique value current step for parallel or serial output.
     *
     *  Respect value of \p parallel_ flag:
     *   - for serial output return actual value of \p current_step
     *   - for parallel output take unique value with account rank and number of processes
     */
    int get_parallel_current_step();


    /**
     * \brief Virtual method for writing data to output file
     */
    virtual int write_data(void) = 0;

    /**
     * Cached MPI rank of process (is tested in methods)
     */
    int rank;

    /**
     * Cached MPI number of processes (is tested in methods)
     */
    int n_proc;

    /**
     * Registered output data. Single map for every value of DiscreteSpace
     * corresponding to nodes, elements and corners.
     */
    OutputDataFieldVec  output_data_vec_[N_DISCRETE_SPACES];

    /**
     * Current step
     */
    int current_step;

    /**
     * The newest time of registered data
     */
    double time;

    /**
     * The last time, when data was wrote to this stream
     */
    double write_time;

    /**
     * Record for current output stream
     */
    Input::Record input_record_;

    /**
     * Base output stream
     */
    ofstream _base_file;

    /**
     * Name of base output file
     */
    FilePath _base_filename;

    /**
     * Name of the equation owning the output stream. Usually the balance equation.
     * Used for forming default output file name and the name of observe output file.
     */
    std::string equation_name_;

    /// Output mesh.
    std::shared_ptr<OutputMeshBase> output_mesh_;
    
    std::shared_ptr<Observe> observe_;

    /// Auxiliary flag for refinement enabling, due to gmsh format.
    bool enable_refinement_;

    /// Parallel or serial version of file format (parallel has effect only for VTK)
    bool parallel_;
};


#endif /* OUTPUT_TIME_HH_ */

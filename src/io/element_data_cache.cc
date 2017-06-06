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
 * @file    element_data_cache.cc
 * @brief
 */


#include "io/element_data_cache.hh"
#include "io/msh_basereader.hh"
#include "system/armadillo_tools.hh"
#include "boost/lexical_cast.hpp"



template <typename T>
ElementDataCache<T>::ElementDataCache()
: ElementDataCacheBase() {}


template <typename T>
ElementDataCache<T>::ElementDataCache(MeshDataHeader data_header, unsigned int size_of_cache, unsigned int row_vec_size) {
	this->time_ = data_header.time;
	this->quantity_name_ = data_header.field_name;
	this->data_ = create_data_cache(size_of_cache, row_vec_size);
}


template <typename T>
ElementDataCache<T>::ElementDataCache(std::string field_name, unsigned int n_rows, unsigned int n_cols, unsigned int size)
{
	this->set_vtk_type<T>();
    this->field_name = field_name;
    this->output_field_name = this->field_name;

    this->n_values = size;

    if (n_cols == 1) {
        if (n_rows == 1) {
            this->n_elem_ = N_SCALAR;
        } else {
            if (n_rows > 1) {
                if (n_rows > 3) {
                    xprintf(PrgErr,
                            "Do not support output of vectors with fixed size >3. Field: %s\n",
                            this->field_name.c_str());
                } else {
                    this->n_elem_ = N_VECTOR;
                }
            } else {
                THROW(ExcOutputVariableVector() << EI_FieldName(this->field_name));
            }
        }
    } else {
        this->n_elem_ = N_TENSOR;
    }

    this->data_ = ElementDataCache<T>::create_data_cache(1, this->n_values * this->n_elem_);
}


template <typename T>
ElementDataCache<T>::~ElementDataCache() {}


template <typename T>
typename ElementDataCache<T>::ComponentDataPtr ElementDataCache<T>::get_component_data(unsigned int component_idx) {
	ASSERT_LT(component_idx, data_.size()).error("Index of component is out of range.\n");
	return data_[component_idx];
}


template <typename T>
typename ElementDataCache<T>::CacheData ElementDataCache<T>::create_data_cache(unsigned int size_of_cache, unsigned int row_vec_size) {
    typename ElementDataCache<T>::CacheData data_cache(size_of_cache);
    for (unsigned int i=0; i<size_of_cache; ++i) {
		typename ElementDataCache<T>::ComponentDataPtr row_vec = std::make_shared<std::vector<T>>();
		row_vec->resize(row_vec_size);
		data_cache[i] = row_vec;
    }

    return data_cache;
}


template <typename T>
void ElementDataCache<T>::read_ascii_data(Tokenizer &tok, unsigned int n_components, unsigned int i_row) {
	unsigned int idx;
	for (unsigned int i_vec=0; i_vec<data_.size(); ++i_vec) {
		idx = i_row * n_components;
		std::vector<T> &vec = *( data_[i_vec].get() );
		for (unsigned int i_col=0; i_col < n_components; ++i_col, ++idx) {
			vec[idx] = boost::lexical_cast<T>(*tok);
			++tok;
		}
	}
}


template <typename T>
void ElementDataCache<T>::read_binary_data(std::istream &data_stream, unsigned int n_components, unsigned int i_row) {
	unsigned int idx;
	for (unsigned int i_vec=0; i_vec<data_.size(); ++i_vec) {
		idx = i_row * n_components;
		std::vector<T> &vec = *( data_[i_vec].get() );
		for (unsigned int i_col=0; i_col < n_components; ++i_col, ++idx) {
			data_stream.read(reinterpret_cast<char *>(&vec[idx]), sizeof(T));
		}
	}
}


/**
 * Output data element on given index @p idx. Method for writing data
 * to output stream.
 *
 * \note This method is used only by MSH file format.
 */
template <typename T>
void ElementDataCache<T>::print_ascii(ostream &out_stream, unsigned int idx)
{
	ASSERT_LT(idx, this->n_values).error();
	std::vector<T> &vec = *( this->data_[0].get() );
	for(unsigned int i = n_elem_*idx; i < n_elem_*(idx+1); ++i )
		out_stream << vec[i] << " ";
}

/**
 * \brief Print all data stored in output data
 *
 * TODO: indicate if the tensor data are output in column-first or raw-first order
 *       and possibly implement transposition. Set such property for individual file formats.
 *       Class OutputData stores always in raw-first order.
 */
template <typename T>
void ElementDataCache<T>::print_ascii_all(ostream &out_stream)
{
    std::vector<T> &vec = *( this->data_[0].get() );
	for(unsigned int idx = 0; idx < this->n_values; idx++) {
    	for(unsigned int i = n_elem_*idx; i < n_elem_*(idx+1); ++i )
    		out_stream << vec[i] << " ";
    }
}


/// Prints the whole data vector into stream.
template <typename T>
void ElementDataCache<T>::print_binary_all(ostream &out_stream, bool print_data_size)
{
	if (print_data_size) {
		// write size of data
		unsigned long long int data_byte_size = this->n_values * n_elem_ * sizeof(T);
		out_stream.write(reinterpret_cast<const char*>(&data_byte_size), sizeof(unsigned long long int));
	}
    // write data
	std::vector<T> &vec = *( this->data_[0].get() );
    for(unsigned int idx = 0; idx < this->n_values; idx++) {
    	for(unsigned int i = n_elem_*idx; i < n_elem_*(idx+1); ++i )
    		out_stream.write(reinterpret_cast<const char*>(&(vec[i])), sizeof(T));
    }
}


template <typename T>
void ElementDataCache<T>::print_all_yaml(ostream &out_stream, unsigned int precision)
{
    out_stream << "[ ";
	std::vector<T> &vec = *( this->data_[0].get() );
    for(unsigned int idx = 0; idx < this->n_values; idx++) {
        if (idx != 0) out_stream << ", ";
        //typename Value::return_type value;
        //out_stream << field_value_to_yaml( Value::from_raw(value, &vec[n_elem_ * idx]), precision );
        out_stream << field_value_to_yaml( vec[n_elem_ * idx], precision );
    }
    out_stream << " ]";
}


template <typename T>
void ElementDataCache<T>::get_min_max_range(double &min, double &max)
{
	min = std::numeric_limits<double>::max();
	max = std::numeric_limits<double>::min();
	std::vector<T> &vec = *( this->data_[0].get() );
    for(unsigned int idx = 0; idx < this->n_values; idx++) {
    	for(unsigned int i = n_elem_*idx; i < n_elem_*(idx+1); ++i ) {
    		if (vec[i] < min) min = vec[i];
    		if (vec[i] > max) max = vec[i];
    	}
    }
}


/**
 * Store data element of given data value under given index.
 */
template <typename T>
void ElementDataCache<T>::store_value(unsigned int idx, const T * value) {
    ASSERT_LT_DBG(idx, this->n_values);
    std::vector<T> &vec = *( this->data_[0].get() );
    unsigned int vec_idx = idx*this->n_elem_;
    for(unsigned int i = 0; i < this->n_elem_; i++, vec_idx++) {
    	vec[vec_idx] = value[i];
    }
};

/**
 * Add value to given index
 */
template <typename T>
void ElementDataCache<T>::add(unsigned int idx, const T * value) {
    ASSERT_LT_DBG(idx, this->n_values);
    std::vector<T> &vec = *( this->data_[0].get() );
    unsigned int vec_idx = idx*this->n_elem_;
    for(unsigned int i = 0; i < this->n_elem_; i++, vec_idx++) {
    	vec[vec_idx] += value[i];
    }
};

/**
 * Reset values at given index
 */
template <typename T>
void ElementDataCache<T>::zero(unsigned int idx) {
    ASSERT_LT_DBG(idx, this->n_values);
    std::vector<T> &vec = *( this->data_[0].get() );
    unsigned int vec_idx = idx*this->n_elem_;
    for(unsigned int i = 0; i < this->n_elem_; i++, vec_idx++) {
    	vec[vec_idx] = 0;
    }
};

/**
 * Normalize values at given index
 */
template <typename T>
void ElementDataCache<T>::normalize(unsigned int idx, unsigned int divisor) {
    ASSERT_LT_DBG(idx, this->n_values);
    std::vector<T> &vec = *( this->data_[0].get() );
    unsigned int vec_idx = idx*this->n_elem_;
    for(unsigned int i = 0; i < this->n_elem_; i++, vec_idx++) {
    	vec[vec_idx] /= divisor;
    }
};



// explicit instantiation of template class
template class ElementDataCache<unsigned int>;
template class ElementDataCache<int>;
template class ElementDataCache<double>;

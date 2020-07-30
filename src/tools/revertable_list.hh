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
 * @file    revertable_list.hh
 * @brief
 */

#ifndef REVARTABLE_LIST_HH_
#define REVARTABLE_LIST_HH_


#include <new>
#include "system/asserts.hh"


/**
 * @brief Struct is a container that encapsulates variable size arrays.
 *
 * Allows to:
 *  1. Add new items to container (method push_back, items are stored as temporary.
 *  2. Mark block of temporary items as final (use method make_permanent)
 *     or cancelled temporary item (method revert_temporary)
 *
 * This algorith allows to add blocks of data, evaluates external condition
 * and possibly reverts unfinished block if condition is not met.
 */
template<class Type>
struct RevertableList {
public:
    /// Constructor, create new instance with reserved size
	RevertableList(std::size_t reserved_size)
    : temporary_size_(0), permanent_size_(0)
    {
        data_.resize(reserved_size);
    }

    /**
     * Resize to new reserved size.
     *
     * New size must be higher than actual size!
     */
    void resize(std::size_t new_size)
    {
    	ASSERT_GT(new_size, reserved_size());
    	data_.resize(new_size);
    }

    /// Return permanent size of list.
    inline std::size_t permanent_size() const
    {
        return permanent_size_;
    }

    /// Return temporary size of list (full size of stored data).
    inline std::size_t temporary_size() const
    {
        return temporary_size_;
    }

    /// Return reserved (maximal) size.
    inline std::size_t reserved_size() const
    {
        return data_.size();
    }

    /// Add new item to list.
    inline std::size_t push_back(const Type &t)
    {
        ASSERT_LT_DBG(temporary_size_, reserved_size()).error("Data array overflowed!\n");
        data_[temporary_size_] = t;
        temporary_size_++;
        return temporary_size_;
    }

    /// Finalize temporary part of data.
    inline std::size_t make_permanent()
    {
    	permanent_size_ = temporary_size_;
        return temporary_size_;
    }

    /// Erase temporary part of data.
    inline std::size_t revert_temporary()
    {
    	temporary_size_ = permanent_size_;
        return temporary_size_;
    }

    /// Clear the list.
    inline void reset()
    {
    	temporary_size_ = 0;
    	permanent_size_ = 0;
    }

    inline typename std::vector<Type>::iterator begin()
    {
    	return data_.begin();
    }

    inline typename std::vector<Type>::iterator end()
    {
    	return data_.begin() + permanent_size_;
    }

    /// Return item on given position
    const Type &operator[](std::size_t pos) const {
        ASSERT_LT_DBG(pos, temporary_size_).error("Position is out of data size!\n");
        return data_[pos];
    }

private:
    std::vector<Type> data_;      ///< Vector of items.
    std::size_t temporary_size_;  ///< Temporary size (full size of used data).
    std::size_t permanent_size_;  ///< Final size of data (part of finalize data).
};

#endif /* REVARTABLE_LIST_HH_ */

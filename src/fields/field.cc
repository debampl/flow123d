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
 * @file    field.cc
 * @brief   
 */

#include "system/exceptions.hh"
#include "mesh/mesh.h"

#include "fields/field_instances.hh"	// for instantiation macros

#include "field.hh"
#include "fields/field.impl.hh"
#include "fields/fe_value_handler.hh"



/****************************************************************************
 *  Instances of field templates
 */



INSTANCE_ALL(Field)

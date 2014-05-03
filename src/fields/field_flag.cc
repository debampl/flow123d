/*
 * field_flags.cc
 *
 *  Created on: May 3, 2014
 *      Author: jb
 */

#include "fields/field_flag.hh"

constexpr FieldFlag::Flags::Mask FieldFlag::equation_input;
constexpr FieldFlag::Flags::Mask FieldFlag::declare_input;
constexpr FieldFlag::Flags::Mask FieldFlag::allow_output;

constexpr FieldFlag::Flags::Mask FieldFlag::in_time_term;
constexpr FieldFlag::Flags::Mask FieldFlag::in_mass_matrix;
constexpr FieldFlag::Flags::Mask FieldFlag::in_right_hand_side;

constexpr FieldFlag::Flags::Mask FieldFlag::equation_result;
